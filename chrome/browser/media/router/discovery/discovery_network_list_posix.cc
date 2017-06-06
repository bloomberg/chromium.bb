// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list.h"

// TODO(btolsch): Remove the preprocessor conditionals when adding Mac version.
#include "build/build_config.h"
#if defined(OS_LINUX)

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"
#include "net/base/net_errors.h"

namespace {

void GetDiscoveryNetworkInfoListImpl(
    const struct ifaddrs* if_list,
    std::vector<DiscoveryNetworkInfo>* network_info_list) {
  std::string ssid;
  for (; if_list != NULL; if_list = if_list->ifa_next) {
    if ((if_list->ifa_flags & IFF_RUNNING) == 0 ||
        (if_list->ifa_flags & IFF_UP) == 0) {
      continue;
    }

    const struct sockaddr* addr = if_list->ifa_addr;
    if (addr->sa_family != AF_PACKET) {
      continue;
    }
    std::string name(if_list->ifa_name);
    if (name.size() == 0) {
      continue;
    }

    // |addr| will always be sockaddr_ll when |sa_family| == AF_PACKET.
    const struct sockaddr_ll* ll_addr =
        reinterpret_cast<const struct sockaddr_ll*>(addr);
    // ARPHRD_ETHER is used to test for Ethernet, as in IEEE 802.3 MAC protocol.
    // This spec is used by both wired Ethernet and wireless (e.g. 802.11).
    if (ll_addr->sll_hatype != ARPHRD_ETHER) {
      continue;
    }

    if (MaybeGetWifiSSID(name, &ssid)) {
      network_info_list->push_back({name, ssid});
      continue;
    }

    if (ll_addr->sll_halen == 0) {
      continue;
    }

    network_info_list->push_back(
        {name, base::HexEncode(ll_addr->sll_addr, ll_addr->sll_halen)});
  }
}

}  // namespace

std::vector<DiscoveryNetworkInfo> GetDiscoveryNetworkInfoList() {
  std::vector<DiscoveryNetworkInfo> network_ids;

  struct ifaddrs* if_list;
  if (getifaddrs(&if_list)) {
    DVLOG(2) << "getifaddrs() error: " << net::ErrorToString(errno);
    return network_ids;
  }

  GetDiscoveryNetworkInfoListImpl(if_list, &network_ids);
  StableSortDiscoveryNetworkInfo(network_ids.begin(), network_ids.end());
  freeifaddrs(if_list);
  return network_ids;
}

#else  // !defined(OS_LINUX)

std::vector<DiscoveryNetworkInfo> GetDiscoveryNetworkInfoList() {
  return std::vector<DiscoveryNetworkInfo>();
}

#endif
