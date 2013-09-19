// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client.h"

#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client_impl.h"

namespace net {

namespace {

const char kMDnsMulticastGroupIPv4[] = "224.0.0.251";
const char kMDnsMulticastGroupIPv6[] = "FF02::FB";

IPEndPoint GetMDnsIPEndPoint(const char* address) {
  IPAddressNumber multicast_group_number;
  bool success = ParseIPLiteralToNumber(address,
                                        &multicast_group_number);
  DCHECK(success);
  return IPEndPoint(multicast_group_number,
                    dns_protocol::kDefaultPortMulticast);
}

}  // namespace

// static
scoped_ptr<MDnsClient> MDnsClient::CreateDefault() {
  return scoped_ptr<MDnsClient>(
      new MDnsClientImpl(MDnsConnection::SocketFactory::CreateDefault()));
}

IPEndPoint GetMDnsIPEndPoint(AddressFamily address_family) {
  switch (address_family) {
  case ADDRESS_FAMILY_IPV4:
    return GetMDnsIPEndPoint(kMDnsMulticastGroupIPv4);
  case ADDRESS_FAMILY_IPV6:
    return GetMDnsIPEndPoint(kMDnsMulticastGroupIPv6);
  default:
    NOTREACHED();
    return IPEndPoint();
  }
}

}  // namespace net
