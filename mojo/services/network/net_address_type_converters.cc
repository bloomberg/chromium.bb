// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "mojo/services/network/net_address_type_converters.h"

namespace mojo {

// static
net::IPEndPoint TypeConverter<net::IPEndPoint, NetAddressPtr>::Convert(
    const NetAddressPtr& obj) {
  if (!obj)
    return net::IPEndPoint();

  switch (obj->family) {
    case NetAddressFamily::IPV4:
      if (!obj->ipv4)
        break;
      return net::IPEndPoint(obj->ipv4->addr.storage(), obj->ipv4->port);

    case NetAddressFamily::IPV6:
      if (!obj->ipv6)
        break;
      return net::IPEndPoint(obj->ipv6->addr.storage(), obj->ipv6->port);

    default:
      break;
  }

  return net::IPEndPoint();
}

// static
NetAddressPtr TypeConverter<NetAddressPtr, net::IPEndPoint>::Convert(
    const net::IPEndPoint& obj) {
  NetAddressPtr net_address(NetAddress::New());

  switch (obj.GetFamily()) {
    case net::ADDRESS_FAMILY_IPV4:
      net_address->family = NetAddressFamily::IPV4;
      net_address->ipv4 = NetAddressIPv4::New();
      net_address->ipv4->port = static_cast<uint16_t>(obj.port());
      net_address->ipv4->addr = Array<uint8_t>::From(obj.address().bytes());
      break;
    case net::ADDRESS_FAMILY_IPV6:
      net_address->ipv6 = NetAddressIPv6::New();
      net_address->family = NetAddressFamily::IPV6;
      net_address->ipv6->port = static_cast<uint16_t>(obj.port());
      net_address->ipv6->addr = Array<uint8_t>::From(obj.address().bytes());
      break;
    default:
      break;
  }

  return net_address;
}

}  // namespace mojo
