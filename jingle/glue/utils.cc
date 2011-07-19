// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/utils.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "third_party/libjingle/source/talk/base/byteorder.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace jingle_glue {

bool IPEndPointToSocketAddress(const net::IPEndPoint& address_chrome,
                               talk_base::SocketAddress* address_lj) {
  if (address_chrome.GetFamily() != AF_INET) {
    LOG(ERROR) << "Only IPv4 addresses are supported.";
    return false;
  }
  uint32 ip_as_int = talk_base::NetworkToHost32(
      *reinterpret_cast<const uint32*>(&address_chrome.address()[0]));
  *address_lj =  talk_base::SocketAddress(ip_as_int, address_chrome.port());
  return true;
}

bool SocketAddressToIPEndPoint(const talk_base::SocketAddress& address_lj,
                               net::IPEndPoint* address_chrome) {
  uint32 ip = talk_base::HostToNetwork32(address_lj.ip());
  net::IPAddressNumber address;
  address.resize(net::kIPv4AddressSize);
  memcpy(&address[0], &ip, net::kIPv4AddressSize);
  *address_chrome = net::IPEndPoint(address, address_lj.port());
  return true;
}

}  // namespace jingle_glue
