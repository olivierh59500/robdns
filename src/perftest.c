#include "adapter.h"
#include "rte-ring.h"
#include "success-failure.h"
#include "db.h"
#include "network.h"
#include "unusedparm.h"
#include "zonefile-parse.h"
#include "zonefile-load.h"
#include "thread.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/******************************************************************************
 ******************************************************************************/
struct TestAdapter
{
    struct Adapter *adapter;
    struct PerfTest *parent;
    unsigned char buf[65536];
    
};

/****************************************************************************
 ****************************************************************************/
struct PerfTest
{
    va_list marker;
    struct Catalog *db;
    struct Thread thread[1];
    struct TestAdapter server;
};

/****************************************************************************
 ****************************************************************************/
static struct Packet 
perftest_alloc_packet(struct Adapter *adapter, struct Thread *thread)
{
    struct TestAdapter *testadapter = (struct TestAdapter *)adapter->userdata;
    struct Packet pkt;
    
    
    pkt.buf = testadapter->buf;
    pkt.max = sizeof(testadapter->buf);
    pkt.offset = 0;
    pkt.fixup.network = 0;
    pkt.fixup.transport = 0;
    
    return pkt;
}


/****************************************************************************
 ****************************************************************************/
void
perftest_server_to_client_response(struct Adapter *adapter,
                                   struct Thread *thread, struct Packet *pkt)
{
    struct TestAdapter *testadapter = (struct TestAdapter *)adapter->userdata;
    struct PerfTest *perftest = testadapter->parent;
    
    UNUSEDPARM(thread);
    UNUSEDPARM(perftest);
    
    
    
}

static const struct DomainPointer example_origin = {(const unsigned char*)"\7example\3com",12};

extern const char *name_of_type(unsigned type);

static const char *perftest_zone[] = {
    "$ORIGIN example.com.     ; designates the start of this zone file in the namespace\r\n",
    "$TTL 1h                  ; default expiration time of all resource records without their own TTL value\r\n",
    "example.com.  IN  SOA  ns.example.com. username.example.com. (\r\n",
    "              2007120710 ; serial number of this zone file\r\n",
    "              1d         ; slave refresh (1 day)\r\n",
    "              2h         ; slave retry time in case of a problem (2 hours)\r\n",
    "              4w         ; slave expiration time (4 weeks)\r\n",
    "              1h         ; maximum caching time in case of failed lookups (1 hour)\r\n",
    "              )\r\n",
    "example.com.  NS    ns                    ; ns.example.com is a nameserver for example.com\r\n",
    "example.com.  NS    ns.somewhere.example. ; ns.somewhere.example is a backup nameserver for example.com\r\n",
    "example.com.  MX    10 mail.example.com.  ; mail.example.com is the mailserver for example.com\r\n",
    "@             MX    20 mail2.example.com. ; equivalent to above line, \"@\" represents zone origin\r\n",
    "@             MX    50 mail3              ; equivalent to above line, but using a relative host name\r\n",
    "example.com.  A     192.0.2.1             ; IPv4 address for example.com\r\n",
    "              AAAA  2001:db8:10::1        ; IPv6 address for example.com\r\n",
    "ns            A     192.0.2.2             ; IPv4 address for ns.example.com\r\n",
    "              AAAA  2001:db8:10::2        ; IPv6 address for ns.example.com\r\n",
    "www           CNAME example.com.          ; www.example.com is an alias for example.com\r\n",
    "wwwtest       CNAME www                   ; wwwtest.example.com is another alias for www.example.com\r\n",
    "mail          A     192.0.2.3             ; IPv4 address for mail.example.com,\r\n",
    "                                          ;  any MX record host must be an address record\r\n",
    "                                          ; as explained in RFC 2181 (section 10.3)\r\n",
    "mail2         A     192.0.2.4             ; IPv4 address for mail2.example.com\r\n",
    "mail3         A     192.0.2.5             ; IPv4 address for mail3.example.com\r\n",
    0
};


/******************************************************************************
 ******************************************************************************/
const char request_template[] = 
"\x00\x26\xf2\xf3\x09\x72\x00\x98\x03\x55\xde\xbe\x08\x00\x45\x00"
"\x00\x3d\x6b\x50\x00\x00\xff\x11\xcc\x66\xc0\xa8\x01\xa7\xc0\xa8"
"\x01\x01\xf9\x82\x00\x35\x00\x29\xd8\xef\x5e\x9f\x01\x00\x00\x01"
"\x00\x00\x00\x00\x00\x00\x03\x77\x77\x77\x07\x65\x78\x61\x6d\x70"
"\x6c\x65\x03\x63\x6f\x6d\x00\x00\x01\x00\x01";

/******************************************************************************
 ******************************************************************************/
int
perftest(int argc, char *argv[])
{
    struct PerfTest perftest[1];
	struct ZoneFileParser *parser;
    struct Catalog *db;
    size_t i;
    
    
    
    /*
     * Create a pseudo-network subsystem for generating packets
     */
    perftest->server.parent = perftest;
    perftest->server.adapter = adapter_create(
                                              perftest_alloc_packet, 
                                              perftest_server_to_client_response, 
                                              &perftest->server);
    adapter_add_ipv4(perftest->server.adapter, 0xC0A80101, 0xFFFFffff);
    
    
    /* create a catalog/database, this is where all the parsed zonefile
     * records will be put */
    perftest->db = catalog_create();
    perftest->thread->catalog = perftest->db;
    db = perftest->db;
    
    /* 
     * Parse a sample zone
     */
    parser = zonefile_begin(
                            example_origin, /* origin */
                            60,             /* TTL */
                            10000,          /* filesize */
                            "<perftest>",   /* filename */
                            zonefile_load,  /* callback */
                            db,             /* callback data */
                            0
                            );
    zonefile_set_singlestep(parser);
    for (i=0; perftest_zone[i]; i++) {
        //printf("%s", perftest_zone[i]);
        zonefile_parse(parser,
                       (const unsigned char*)perftest_zone[i],
                       strlen(perftest_zone[i])
                       );
        //zonefile_flush(parser);
    }
    zonefile_end(parser);
    
    /*
     * Send packets
     */
    for (i=0; i<1000000; i++) {
        struct Frame frame[1];

        network_receive(
                        frame,
                        perftest->thread,
                        perftest->server.adapter,
                        0,
                        0,
                        (unsigned char*)request_template,
                        sizeof(request_template)-1);
    }

    exit(1);
    return 0;
}

