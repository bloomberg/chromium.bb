// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/netbios_host_locator.h"

#include "net/base/network_change_notifier.h"

namespace chromeos {
namespace smb_client {

net::IPAddress CalculateBroadcastAddress(
    const net::NetworkInterface& interface) {
  net::IPAddress internet_address = interface.address;
  const uint32_t inverted_net_mask = 0xffffffff >> interface.prefix_length;

  net::IPAddressBytes bytes = internet_address.bytes();

  uint8_t b0 = bytes[0] | (inverted_net_mask >> 24);
  uint8_t b1 = bytes[1] | (inverted_net_mask >> 16);
  uint8_t b2 = bytes[2] | (inverted_net_mask >> 8);
  uint8_t b3 = bytes[3] | (inverted_net_mask);

  net::IPAddress broadcast_address(b0, b1, b2, b3);
  return broadcast_address;
}

bool ShouldUseInterface(const net::NetworkInterface& interface) {
  return interface.address.IsIPv4() &&
         (interface.type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
          interface.type == net::NetworkChangeNotifier::CONNECTION_WIFI);
}

NetBiosHostLocator::NetBiosHostLocator(GetInterfacesFunction get_interfaces)
    : get_interfaces_(std::move(get_interfaces)) {}

NetBiosHostLocator::~NetBiosHostLocator() = default;

void NetBiosHostLocator::FindHosts(FindHostsCallback callback) {
  DCHECK(callback);

  callback_ = std::move(callback);
}

net::NetworkInterfaceList NetBiosHostLocator::GetNetworkInterfaceList() {
  return get_interfaces_.Run();
}

}  // namespace smb_client
}  // namespace chromeos
