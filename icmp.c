/* icmp.c - ICMP support for sendip
 * Author: Mike Ricketts <mike@earth.li>
 */

#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "sendip_module.h"
#include "icmp.h"
#include "ipv4.h"
#include "ipv6.h"

/* Character that identifies our options
 */
const char opt_char='c';

static void icmpcsum(sendip_data *icmp_hdr, sendip_data *data) {
	icmp_header *icp = (icmp_header *)icmp_hdr->data;
	u_int8_t *tempbuf = malloc(icmp_hdr->alloc_len+data->alloc_len);
	icp->check = 0;
	memcpy(tempbuf,icmp_hdr->data,icmp_hdr->alloc_len);
	memcpy(tempbuf+icmp_hdr->alloc_len,data->data,data->alloc_len);
	icp->check = csum((u_int16_t *)tempbuf,icmp_hdr->alloc_len+data->alloc_len);
}

static void icmp6csum(struct in6_addr *src, struct in6_addr *dst,
		    sendip_data *hdr, sendip_data *data) {
	
	icmp_header *icp = (icmp_header *)hdr->data;
	u_int8_t *tempbuf = malloc(hdr->alloc_len+data->alloc_len);
	icp->check = 0;
	memcpy(tempbuf, hdr->data, hdr->alloc_len);
	memcpy(tempbuf + hdr->alloc_len, data->data, data->alloc_len);
	icp->check = ipv6_csum(src, dst, IPPROTO_ICMPV6,
			       (u_int16_t *)tempbuf,
			       hdr->alloc_len + data->alloc_len);
}

sendip_data *initialize(void) {
	sendip_data *ret = malloc(sizeof(sendip_data));
	icmp_header *icmp = malloc(sizeof(icmp_header));
	memset(icmp,0,sizeof(icmp_header));
	ret->alloc_len = sizeof(icmp_header);
	ret->data = (void *)icmp;
	ret->modified=0;
	return ret;
}

bool do_opt(char *opt, char *arg, sendip_data *pack) {
	icmp_header *icp = (icmp_header *)pack->data;
	switch(opt[1]) {
	case 't':
		icp->type = (u_int8_t)strtoul(arg, (char **)NULL, 0);
		pack->modified |= ICMP_MOD_TYPE;
		break;
	case 'd':
		icp->code = (u_int8_t)strtoul(arg, (char **)NULL, 0);
		pack->modified |= ICMP_MOD_CODE;
		break;
	case 'c':
		icp->check = htons((u_int16_t)strtoul(arg, (char **)NULL, 0));
		pack->modified |= ICMP_MOD_CHECK;
		break;
	}
	return TRUE;

}

bool finalize(char *hdrs, sendip_data *headers[], sendip_data *data,
				  sendip_data *pack) {
	icmp_header *icp = (icmp_header *)pack->data;
	int i, foundit=0;
	int num_hdrs = strlen(hdrs);


	/* Find enclosing IP header and do the checksum */
	for(i=num_hdrs;i>0;i--) {
		if(hdrs[i-1]=='i' || hdrs[i-1]=='6') {
			foundit=1; break;
		}
	}
	if(foundit) {
		i--;
		if(hdrs[i]=='i') {
			if(!(headers[i]->modified&IP_MOD_PROTOCOL)) {
				((ip_header *)(headers[i]->data))->protocol=IPPROTO_ICMP;
				headers[i]->modified |= IP_MOD_PROTOCOL;
			}
		} else {  // ipv6
			if(!(headers[i]->modified&IPV6_MOD_NXT)) {
				((ipv6_header *)(headers[i]->data))->ip6_nxt=IPPROTO_ICMPV6;
				headers[i]->modified |= IPV6_MOD_NXT;
			}
		}
	}
		
	if(!(pack->modified&ICMP_MOD_TYPE)) {
		if(hdrs[i]=='6') {
			icp->type=ICMP6_ECHO_REQUEST;
		} else {
			icp->type=ICMP_ECHO;
		}
	}
	if(!(pack->modified&ICMP_MOD_CHECK)) {
		if (hdrs[i] == '6') {
			struct in6_addr *src, *dst;
			src = (struct in6_addr *)&(((ipv6_header *)(headers[i]->data))->ip6_src);
			dst = (struct in6_addr *)&(((ipv6_header *)(headers[i]->data))->ip6_dst);
			icmp6csum(src, dst, pack, data);
		} else {
			icmpcsum(pack,data);
		}
	}
	return TRUE;
}

int num_opts() {
	return sizeof(icmp_opts)/sizeof(sendip_option); 
}
sendip_option *get_opts() {
	return icmp_opts;
}
char get_optchar() {
	return opt_char;
}