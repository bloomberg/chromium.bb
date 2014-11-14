// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util_linux.h"

#include <net/if.h>
#include <netinet/in.h>
#include <set>
#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/address_tracker_linux.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util_posix.h"
#include "url/gurl.h"

namespace net {

namespace {

// When returning true, the platform native IPv6 address attributes were
// successfully converted to net IP address attributes. Otherwise, returning
// false and the caller should drop the IP address which can't be used by the
// application layer.
bool TryConvertNativeToNetIPAttributes(int native_attributes,
                                       int* net_attributes) {
  // For Linux/ChromeOS/Android, we disallow addresses with attributes
  // IFA_F_OPTIMISTIC, IFA_F_DADFAILED, and IFA_F_TENTATIVE as these
  // are still progressing through duplicated address detection (DAD)
  // and shouldn't be used by the application layer until DAD process
  // is completed.
  if (native_attributes & (
#if !defined(OS_ANDROID)
                              IFA_F_OPTIMISTIC | IFA_F_DADFAILED |
#endif  // !OS_ANDROID
                              IFA_F_TENTATIVE)) {
    return false;
  }

  if (native_attributes & IFA_F_TEMPORARY) {
    *net_attributes |= IP_ADDRESS_ATTRIBUTE_TEMPORARY;
  }

  if (native_attributes & IFA_F_DEPRECATED) {
    *net_attributes |= IP_ADDRESS_ATTRIBUTE_DEPRECATED;
  }

  return true;
}

}  // namespace

namespace internal {

inline const unsigned char* GetIPAddressData(const IPAddressNumber& ip) {
#if defined(OS_ANDROID)
  return ip.begin();
#else
  return ip.data();
#endif
}

bool GetNetworkListImpl(
    NetworkInterfaceList* networks,
    int policy,
    const base::hash_set<int>& online_links,
    const internal::AddressTrackerLinux::AddressMap& address_map,
    GetInterfaceNameFunction get_interface_name) {
  std::map<int, std::string> ifnames;

  for (internal::AddressTrackerLinux::AddressMap::const_iterator it =
           address_map.begin();
       it != address_map.end(); ++it) {
    // Ignore addresses whose links are not online.
    if (online_links.find(it->second.ifa_index) == online_links.end())
      continue;

    sockaddr_storage sock_addr;
    socklen_t sock_len = sizeof(sockaddr_storage);

    // Convert to sockaddr for next check.
    if (!IPEndPoint(it->first, 0)
             .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addr), &sock_len)) {
      continue;
    }

    // Skip unspecified addresses (i.e. made of zeroes) and loopback addresses
    if (IsLoopbackOrUnspecifiedAddress(reinterpret_cast<sockaddr*>(&sock_addr)))
      continue;

    int ip_attributes = IP_ADDRESS_ATTRIBUTE_NONE;

    if (it->second.ifa_family == AF_INET6) {
      // Ignore addresses whose attributes are not actionable by
      // the application layer.
      if (!TryConvertNativeToNetIPAttributes(it->second.ifa_flags,
                                             &ip_attributes))
        continue;
    }

    // Find the name of this link.
    std::map<int, std::string>::const_iterator itname =
        ifnames.find(it->second.ifa_index);
    std::string ifname;
    if (itname == ifnames.end()) {
      char buffer[IF_NAMESIZE] = {0};
      if (get_interface_name(it->second.ifa_index, buffer)) {
        ifname = ifnames[it->second.ifa_index] = buffer;
      } else {
        // Ignore addresses whose interface name can't be retrieved.
        continue;
      }
    } else {
      ifname = itname->second;
    }

    // Based on the interface name and policy, determine whether we
    // should ignore it.
    if (ShouldIgnoreInterface(ifname, policy))
      continue;

    networks->push_back(
        NetworkInterface(ifname, ifname, it->second.ifa_index,
                         NetworkChangeNotifier::CONNECTION_UNKNOWN, it->first,
                         it->second.ifa_prefixlen, ip_attributes));
  }

  return true;
}

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  if (networks == NULL)
    return false;

  internal::AddressTrackerLinux tracker;
  tracker.Init();

  return internal::GetNetworkListImpl(networks, policy,
                                      tracker.GetOnlineLinks(),
                                      tracker.GetAddressMap(), &if_indextoname);
}

}  // namespace net
