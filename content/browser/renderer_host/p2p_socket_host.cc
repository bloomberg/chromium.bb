// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p_socket_host.h"

#include "content/browser/renderer_host/p2p_socket_host_udp.h"

P2PSocketHost::P2PSocketHost(P2PSocketsHost* host, int routing_id, int id)
    : host_(host), routing_id_(routing_id), id_(id) {
}

P2PSocketHost::~P2PSocketHost() { }

// static
P2PSocketHost* P2PSocketHost::Create(
    P2PSocketsHost* host, int routing_id, int id, P2PSocketType type) {
  switch (type) {
    case P2P_SOCKET_UDP:
      return new P2PSocketHostUdp(host, routing_id, id);

    case P2P_SOCKET_TCP_SERVER:
      // TODO(sergeyu): Implement TCP sockets support.
      return NULL;

    case P2P_SOCKET_TCP_CLIENT:
      return NULL;
  }

  NOTREACHED();
  return NULL;
}
