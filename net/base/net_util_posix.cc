// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <set>
#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if !defined(OS_NACL)
#if defined(OS_MACOSX)
#include <ifaddrs.h>
#else
#include "net/base/address_tracker_linux.h"
#include "net/base/net_util_posix.h"
#endif  // OS_MACOSX
#include <net/if.h>
#include <netinet/in.h>
#endif  // !defined(OS_NACL)

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <net/if_media.h>
#include <netinet/in_var.h>
#include <sys/ioctl.h>
#endif

namespace net {

namespace {

// The application layer can pass |policy| defined in net_util.h to
// request filtering out certain type of interfaces.
bool ShouldIgnoreInterface(const std::string& name, int policy) {
  // Filter out VMware interfaces, typically named vmnet1 and vmnet8,
  // which might not be useful for use cases like WebRTC.
  if ((policy & EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES) &&
      ((name.find("vmnet") != std::string::npos) ||
       (name.find("vnic") != std::string::npos))) {
    return true;
  }

  return false;
}

// Check if the address is unspecified (i.e. made of zeroes) or loopback.
bool IsLoopbackOrUnspecifiedAddress(const sockaddr* addr) {
  if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6* addr_in6 =
        reinterpret_cast<const struct sockaddr_in6*>(addr);
    const struct in6_addr* sin6_addr = &addr_in6->sin6_addr;
    if (IN6_IS_ADDR_LOOPBACK(sin6_addr) || IN6_IS_ADDR_UNSPECIFIED(sin6_addr)) {
      return true;
    }
  } else if (addr->sa_family == AF_INET) {
    const struct sockaddr_in* addr_in =
        reinterpret_cast<const struct sockaddr_in*>(addr);
    if (addr_in->sin_addr.s_addr == INADDR_LOOPBACK ||
        addr_in->sin_addr.s_addr == 0) {
      return true;
    }
  } else {
    // Skip non-IP addresses.
    return true;
  }
  return false;
}

#if defined(OS_MACOSX)

struct NetworkInterfaceInfo {
  NetworkInterfaceInfo() : permanent(true) { }

  bool permanent;  // IPv6 has notion of temporary address. If the address is
                   // IPv6 and it's temporary this field will be false.
  NetworkInterface interface;
};

// This method will remove permanent IPv6 addresses if a temporary address
// is available for same network interface.
void RemovePermanentIPv6AddressesWhereTemporaryExists(
    std::vector<NetworkInterfaceInfo>* infos) {
  if (!infos || infos->empty())
    return;

  // Build a set containing the names of interfaces with a temp IPv6 address
  std::set<std::string> ifaces_with_temp_addrs;
  std::vector<NetworkInterfaceInfo>::iterator i;
  for (i = infos->begin(); i != infos->end(); ++i) {
    if (!i->permanent && i->interface.address.size() == kIPv6AddressSize) {
      ifaces_with_temp_addrs.insert(i->interface.name);
    }
  }

  // If there are no such interfaces then there's no further work.
  if (ifaces_with_temp_addrs.empty())
    return;

  // Search for permenent addresses belonging to same network interface.
  for (i = infos->begin(); i != infos->end(); ) {
    // If the address is IPv6 and it's permanent and there is temporary
    // address for it, then we can remove this address.
    if ((i->interface.address.size() == kIPv6AddressSize) && i->permanent &&
        (ifaces_with_temp_addrs.find(i->interface.name) !=
            ifaces_with_temp_addrs.end())) {
      i = infos->erase(i);
    } else {
      ++i;
    }
  }
}

#if !defined(OS_IOS)
NetworkChangeNotifier::ConnectionType GetNetworkInterfaceType(
    int addr_family, const std::string& interface_name) {
  NetworkChangeNotifier::ConnectionType type =
      NetworkChangeNotifier::CONNECTION_UNKNOWN;

  struct ifmediareq ifmr = {};
  strncpy(ifmr.ifm_name, interface_name.c_str(), sizeof(ifmr.ifm_name) - 1);

  int s = socket(addr_family, SOCK_DGRAM, 0);
  if (s == -1) {
    return type;
  }

  if (ioctl(s, SIOCGIFMEDIA, &ifmr) != -1) {
    if (ifmr.ifm_current & IFM_IEEE80211) {
      type = NetworkChangeNotifier::CONNECTION_WIFI;
    } else if (ifmr.ifm_current & IFM_ETHER) {
      type = NetworkChangeNotifier::CONNECTION_ETHERNET;
    }
  }
  close(s);
  return type;
}

#endif                   // !defined(OS_IOS)
#elif !defined(OS_NACL)  // OS_MACOSX

// Convert platform native IPv6 address attributes to net IP address
// attributes and drop ones that can't be used by the application
// layer.
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
#endif  // OS_MACOSX
}  // namespace

namespace internal {

#if !defined(OS_MACOSX) && !defined(OS_NACL)

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
       it != address_map.end();
       ++it) {
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
        NetworkInterface(ifname,
                         ifname,
                         it->second.ifa_index,
                         NetworkChangeNotifier::CONNECTION_UNKNOWN,
                         it->first,
                         it->second.ifa_prefixlen,
                         ip_attributes));
  }

  return true;
}
#endif

}  // namespace internal

bool GetNetworkList(NetworkInterfaceList* networks, int policy) {
  if (networks == NULL)
    return false;
#if defined(OS_NACL)
  NOTIMPLEMENTED();
  return false;
#elif !defined(OS_MACOSX)

  internal::AddressTrackerLinux tracker;
  tracker.Init();

  return internal::GetNetworkListImpl(networks,
                                      policy,
                                      tracker.GetOnlineLinks(),
                                      tracker.GetAddressMap(),
                                      &if_indextoname);

#else  // Only OS_MACOSX and OS_IOS will run the code below

  // getifaddrs() may require IO operations.
  base::ThreadRestrictions::AssertIOAllowed();

#if !defined(OS_IOS)
  int ioctl_socket = -1;
  if (policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE) {
    // we need a socket to query information about temporary address.
    ioctl_socket = socket(AF_INET6, SOCK_DGRAM, 0);
    DCHECK_GT(ioctl_socket, 0);
  }
#endif

  ifaddrs* interfaces;
  if (getifaddrs(&interfaces) < 0) {
    PLOG(ERROR) << "getifaddrs";
    return false;
  }

  std::vector<NetworkInterfaceInfo> network_infos;

  // Enumerate the addresses assigned to network interfaces which are up.
  for (ifaddrs *interface = interfaces;
       interface != NULL;
       interface = interface->ifa_next) {
    // Skip loopback interfaces, and ones which are down.
    if (!(IFF_UP & interface->ifa_flags))
      continue;
    if (IFF_LOOPBACK & interface->ifa_flags)
      continue;
    // Skip interfaces with no address configured.
    struct sockaddr* addr = interface->ifa_addr;
    if (!addr)
      continue;

    // Skip unspecified addresses (i.e. made of zeroes) and loopback addresses
    // configured on non-loopback interfaces.
    if (IsLoopbackOrUnspecifiedAddress(addr))
      continue;

    int addr_size = 0;
    if (addr->sa_family == AF_INET6) {
      addr_size = sizeof(sockaddr_in6);
    } else if (addr->sa_family == AF_INET) {
      addr_size = sizeof(sockaddr_in);
    }

    const std::string& name = interface->ifa_name;
    // Filter out VMware interfaces, typically named vmnet1 and vmnet8.
    if (ShouldIgnoreInterface(name, policy)) {
      continue;
    }

    NetworkInterfaceInfo network_info;
    NetworkChangeNotifier::ConnectionType connection_type =
        NetworkChangeNotifier::CONNECTION_UNKNOWN;
#if !defined(OS_IOS)
    // Check if this is a temporary address. Currently this is only supported
    // on Mac.
    if ((policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE) &&
        ioctl_socket >= 0 && addr->sa_family == AF_INET6) {
      struct in6_ifreq ifr = {};
      strncpy(ifr.ifr_name, interface->ifa_name, sizeof(ifr.ifr_name) - 1);
      memcpy(&ifr.ifr_ifru.ifru_addr, interface->ifa_addr,
             interface->ifa_addr->sa_len);
      int rv = ioctl(ioctl_socket, SIOCGIFAFLAG_IN6, &ifr);
      if (rv >= 0) {
        network_info.permanent = !(ifr.ifr_ifru.ifru_flags & IN6_IFF_TEMPORARY);
      }
    }

    connection_type = GetNetworkInterfaceType(addr->sa_family, name);
#endif

    IPEndPoint address;
    if (address.FromSockAddr(addr, addr_size)) {
      uint8 net_mask = 0;
      if (interface->ifa_netmask) {
        // If not otherwise set, assume the same sa_family as ifa_addr.
        if (interface->ifa_netmask->sa_family == 0) {
          interface->ifa_netmask->sa_family = addr->sa_family;
        }
        IPEndPoint netmask;
        if (netmask.FromSockAddr(interface->ifa_netmask, addr_size)) {
          net_mask = MaskPrefixLength(netmask.address());
        }
      }
      network_info.interface = NetworkInterface(name,
                                                name,
                                                if_nametoindex(name.c_str()),
                                                connection_type,
                                                address.address(),
                                                net_mask,
                                                IP_ADDRESS_ATTRIBUTE_NONE);

      network_infos.push_back(NetworkInterfaceInfo(network_info));
    }
  }
  freeifaddrs(interfaces);
#if !defined(OS_IOS)
  if (ioctl_socket >= 0) {
    close(ioctl_socket);
  }
#endif

  if (policy & INCLUDE_ONLY_TEMP_IPV6_ADDRESS_IF_POSSIBLE) {
    RemovePermanentIPv6AddressesWhereTemporaryExists(&network_infos);
  }

  for (size_t i = 0; i < network_infos.size(); ++i) {
    networks->push_back(network_infos[i].interface);
  }
  return true;
#endif
}

WifiPHYLayerProtocol GetWifiPHYLayerProtocol() {
  return WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

scoped_ptr<ScopedWifiOptions> SetWifiOptions(int options) {
  return scoped_ptr<ScopedWifiOptions>();
}


}  // namespace net
