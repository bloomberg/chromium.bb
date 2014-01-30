// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines some basic types used by the P2P-related IPC
// messages.

#ifndef CONTENT_PUBLIC_COMMON_P2P_SOCKET_TYPE_H_
#define CONTENT_PUBLIC_COMMON_P2P_SOCKET_TYPE_H_

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

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_P2P_SOCKET_TYPE_H_
