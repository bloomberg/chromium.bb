// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/netbios_host_locator.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/chromeos/smb_client/smb_constants.h"
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
    : NetBiosHostLocator(get_interfaces,
                         client_factory,
                         std::make_unique<base::OneShotTimer>()) {}

NetBiosHostLocator::NetBiosHostLocator(
    GetInterfacesFunction get_interfaces,
    NetBiosClientFactory client_factory,
    std::unique_ptr<base::OneShotTimer> timer)
    : get_interfaces_(std::move(get_interfaces)),
      client_factory_(std::move(client_factory)),
      timer_(std::move(timer)) {}

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

  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kNetBiosDiscoveryTimeoutSeconds),
                this, &NetBiosHostLocator::StopDiscovery);
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

void NetBiosHostLocator::StopDiscovery() {
  discovery_done_ = true;
  netbios_clients_.clear();

  if (outstanding_parse_requests_ == 0) {
    FinishFindHosts();
  }
}

void NetBiosHostLocator::FinishFindHosts() {
  std::move(callback_).Run(true /* success */, results_);
  ResetHostLocator();
}

void NetBiosHostLocator::ResetHostLocator() {
  DCHECK(outstanding_parse_requests_ = 0);
  DCHECK(netbios_clients_.empty());

  results_.clear();
  discovery_done_ = false;
  running_ = false;
}

}  // namespace smb_client
}  // namespace chromeos
