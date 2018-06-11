// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/netbios_client.h"

#include "chromeos/network/firewall_hole.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace chromeos {
namespace smb_client {

namespace {

// TODO(baileyberro): Fill out chrome_policy with the enterprise policy to
// disable NETBIOS discovery. https://crbug.com/850966
constexpr net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("smb_netbios_name_query", R"(
        semantics {
          sender: "Native SMB for ChromeOS"
          description:
            "Performs a NETBIOS Name Query Request on the network to find "
            "discoverable file shares."
          trigger: " Starting the File Share mount process."
          data:
            "A NETBIOS Name Query Request packet as defined by "
            "RFC 1002 Section 4.2.12."
          destination: OTHER
          destination_other:
            "Data is sent to the broadcast_address of the local network."
        }
        policy {
          cookies_allowed: NO
          setting:
            "No settings control. This request will not be sent if the user "
            "does not attempt to mount a Network File Share."
          policy_exception_justification: "Not Implemented."
        })");
}

}  // namespace

NetBiosClient::NetBiosClient(network::mojom::NetworkContext* network_context) {
  NOTIMPLEMENTED();

  GetNetworkTrafficAnnotationTag();
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
  // https://tools.ietf.org/html/rfc1002
  // Section 4.2.12
  // [0-1]    - Transaction Id.
  // [2-3]    - Broadcast Flag.
  // [4-5]    - Question Count.
  // [6-7]    - Answer Resource Count.
  // [8-9]    - Authority Resource Count.
  // [10-11]  - Additional Resource Count.
  // [12]     - Length of name. 16 bytes of name encoded to 32 bytes.
  // [13-14]  - '*' character encodes to 2 bytes.
  // [15-44]  - Reaming 15 nulls which encode as 30 * 0x41.
  // [45]     - Length of next segment.
  // [46-47]  - Question type: Node status.
  // [48-49]  - Question clasS: Internet.
  std::vector<uint8_t> packet = {
      0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x20, 0x43, 0x4b, 0x41, 0x41, 0x41, 0x41, 0x41,
      0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
      0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
      0x41, 0x41, 0x41, 0x41, 0x41, 0x00, 0x00, 0x21, 0x00, 0x01};

  // Set Transaction ID in Big Endian representation.
  uint8_t first_byte_tx_id = transaction_id_ >> 8;
  uint8_t second_byte_tx_id = transaction_id_ & 0xFF;

  packet[0] = first_byte_tx_id;
  packet[1] = second_byte_tx_id;

  return packet;
}

}  // namespace smb_client
}  // namespace chromeos
