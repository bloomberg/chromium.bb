// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some basic types used by the P2P-related IPC
// messages.

#ifndef CONTENT_COMMON_P2P_SOCKET_TYPE_H_
#define CONTENT_COMMON_P2P_SOCKET_TYPE_H_

#include <string>

#include "net/base/ip_endpoint.h"

namespace content {

enum P2PSocketOption {
  P2P_SOCKET_OPT_RCVBUF,  // Receive buffer size.
  P2P_SOCKET_OPT_SNDBUF,  // Send buffer size.
  P2P_SOCKET_OPT_DSCP,    // DSCP code.
  P2P_SOCKET_OPT_MAX
};

// Type of P2P Socket.
enum P2PSocketType {
  P2P_SOCKET_UDP,
  P2P_SOCKET_TCP_SERVER,
  P2P_SOCKET_STUN_TCP_SERVER,
  P2P_SOCKET_TCP_CLIENT,
  P2P_SOCKET_STUN_TCP_CLIENT,
  P2P_SOCKET_SSLTCP_CLIENT,
  P2P_SOCKET_STUN_SSLTCP_CLIENT,
  P2P_SOCKET_TLS_CLIENT,
  P2P_SOCKET_STUN_TLS_CLIENT,
  P2P_SOCKET_TYPE_LAST = P2P_SOCKET_STUN_TLS_CLIENT
};

// Struct which carries both resolved IP address and host string literal.
// Port number will be part of |ip_address|.
struct P2PHostAndIPEndPoint {
  P2PHostAndIPEndPoint() {}
  P2PHostAndIPEndPoint(const std::string& hostname,
                       const net::IPEndPoint& ip_address)
      : hostname(hostname), ip_address(ip_address) {
  }

  std::string hostname;
  net::IPEndPoint ip_address;
};

// Stuct which keeps track of metrics during a send operation on P2P sockets.
// Currently, it only carries packet_id but could be expanded to include
// timestamps when packet arrives at various points.
struct P2PSendPacketMetrics {
  P2PSendPacketMetrics() : packet_id(0) {}
  explicit P2PSendPacketMetrics(uint64_t packet_id) : packet_id(packet_id) {}

  uint64_t packet_id;
};

}  // namespace content

#endif  // CONTENT_COMMON_P2P_SOCKET_TYPE_H_
