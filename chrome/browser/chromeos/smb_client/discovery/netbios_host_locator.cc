// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/netbios_host_locator.h"

#include <memory>

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

NetBiosHostLocator::NetBiosHostLocator(GetInterfacesFunction get_interfaces,
                                       NetBiosClientFactory client_factory)
    : get_interfaces_(std::move(get_interfaces)),
      client_factory_(std::move(client_factory)) {}

NetBiosHostLocator::~NetBiosHostLocator() = default;

void NetBiosHostLocator::FindHosts(FindHostsCallback callback) {
  DCHECK(!running_);
  DCHECK(callback);
  callback_ = std::move(callback);
  running_ = true;

  net::NetworkInterfaceList network_interface_list = GetNetworkInterfaceList();

  for (const auto& interface : network_interface_list) {
    if (ShouldUseInterface(interface)) {
      FindHostsOnInterface(interface);
    }
  }

  if (netbios_clients_.empty()) {
    // No NetBiosClients were created since there were either no interfaces or
    // no valid interfaces.
    running_ = false;
    std::move(callback_).Run(false /* success */, results_);
  }
}

net::NetworkInterfaceList NetBiosHostLocator::GetNetworkInterfaceList() {
  return get_interfaces_.Run();
}

void NetBiosHostLocator::FindHostsOnInterface(
    const net::NetworkInterface& interface) {
  net::IPAddress broadcast_address = CalculateBroadcastAddress(interface);

  netbios_clients_.push_back(CreateClient());
  ExecuteNameRequest(broadcast_address);
}

std::unique_ptr<NetBiosClientInterface> NetBiosHostLocator::CreateClient()
    const {
  return client_factory_.Run();
}

void NetBiosHostLocator::ExecuteNameRequest(
    const net::IPAddress& broadcast_address) {
  netbios_clients_.back()->ExecuteNameRequest(
      broadcast_address, transaction_id_++,
      base::BindRepeating(&NetBiosHostLocator::PacketReceived,
                          base::Unretained(this)));
}

void NetBiosHostLocator::PacketReceived(const std::vector<uint8_t>& packet,
                                        uint16_t transaction_id,
                                        const net::IPEndPoint& sender_ip) {
  NOTREACHED();
}

}  // namespace smb_client
}  // namespace chromeos
