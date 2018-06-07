// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/netbios_client.h"

#include "chromeos/network/firewall_hole.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace chromeos {
namespace smb_client {

NetBiosClient::NetBiosClient(network::mojom::NetworkContext* network_context) {
  NOTIMPLEMENTED();
}

NetBiosClient::~NetBiosClient() {}

void NetBiosClient::ExecuteNameRequest(const net::IPAddress& broadcast_address,
                                       uint16_t transaction_id,
                                       NetBiosResponseCallback callback) {
  NOTIMPLEMENTED();
}

void NetBiosClient::BindSocket() {
  NOTIMPLEMENTED();
}

void NetBiosClient::OpenPort(uint16_t port) {
  NOTIMPLEMENTED();
}

void NetBiosClient::SetBroadcast() {
  NOTIMPLEMENTED();
}

void NetBiosClient::SendPacket() {
  NOTIMPLEMENTED();
}

void NetBiosClient::OnBindComplete(
    int32_t result,
    const base::Optional<net::IPEndPoint>& local_ip) {
  NOTIMPLEMENTED();
}

void NetBiosClient::OnOpenPortComplete(
    std::unique_ptr<FirewallHole> firewall_hole) {
  NOTIMPLEMENTED();
}

void NetBiosClient::OnSetBroadcastCompleted(int32_t result) {
  NOTIMPLEMENTED();
}

void NetBiosClient::OnSendCompleted(int32_t result) {
  NOTIMPLEMENTED();
}

void NetBiosClient::OnReceived(int32_t result,
                               const base::Optional<net::IPEndPoint>& src_ip,
                               base::Optional<base::span<const uint8_t>> data) {
  NOTIMPLEMENTED();
}

std::vector<uint8_t> NetBiosClient::GenerateBroadcastPacket() {
  NOTIMPLEMENTED();
  return std::vector<uint8_t>();
}

}  // namespace smb_client
}  // namespace chromeos
