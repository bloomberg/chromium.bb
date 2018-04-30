// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_interfaces.h"

#include <netstack/netconfig.h>

#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces_posix.h"

namespace net {

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s <= 0) {
    PLOG(ERROR) << "socket";
    return false;
  }

  uint32_t num_ifs = 0;
  if (ioctl_netc_get_num_ifs(s, &num_ifs) < 0) {
    PLOG(ERROR) << "ioctl_netc_get_num_ifs";
    PCHECK(close(s) == 0);
    return false;
  }

  for (uint32_t i = 0; i < num_ifs; ++i) {
    netc_if_info_t interface;

    if (ioctl_netc_get_if_info_at(s, &i, &interface) < 0) {
      PLOG(WARNING) << "ioctl_netc_get_if_info_at";
      continue;
    }

    // Skip loopback addresses.
    if (internal::IsLoopbackOrUnspecifiedAddress(
            reinterpret_cast<sockaddr*>(&(interface.addr)))) {
      continue;
    }

    IPEndPoint address;
    if (!address.FromSockAddr(reinterpret_cast<sockaddr*>(&(interface.addr)),
                              sizeof(interface.addr))) {
      DLOG(WARNING) << "ioctl_netc_get_if_info returned invalid address.";
      continue;
    }

    int prefix_length = 0;
    IPEndPoint netmask;
    if (netmask.FromSockAddr(reinterpret_cast<sockaddr*>(&(interface.netmask)),
                             sizeof(interface.netmask))) {
      prefix_length = MaskPrefixLength(netmask.address());
    }

    // TODO(sergeyu): attributes field is used to return address state for IPv6
    // addresses. Currently ioctl_netc_get_if_info doesn't provide this
    // information.
    int attributes = 0;

    networks->push_back(
        NetworkInterface(interface.name, interface.name, interface.index,
                         NetworkChangeNotifier::CONNECTION_UNKNOWN,
                         address.address(), prefix_length, attributes));
  }

  PCHECK(close(s) == 0);

  return true;
}

std::string GetWifiSSID() {
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace net
