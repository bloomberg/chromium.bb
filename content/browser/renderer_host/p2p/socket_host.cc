// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host.h"

#include "net/base/sys_byteorder.h"
#include "content/browser/renderer_host/p2p/socket_host_tcp.h"
#include "content/browser/renderer_host/p2p/socket_host_tcp_server.h"
#include "content/browser/renderer_host/p2p/socket_host_udp.h"

namespace {
const int kStunHeaderSize = 20;
const uint32 kStunMagicCookie = 0x2112A442;
}  // namespace

namespace content {

P2PSocketHost::P2PSocketHost(IPC::Message::Sender* message_sender,
                             int routing_id, int id)
    : message_sender_(message_sender),
      routing_id_(routing_id),
      id_(id),
      state_(STATE_UNINITIALIZED) {
}

P2PSocketHost::~P2PSocketHost() { }

// Verifies that the packet |data| has a valid STUN header.
// static
bool P2PSocketHost::GetStunPacketType(
    const char* data, int data_size, StunMessageType* type) {

  if (data_size < kStunHeaderSize)
    return false;

  uint32 cookie = ntohl(*reinterpret_cast<const uint32*>(data + 4));
  if (cookie != kStunMagicCookie)
    return false;

  uint16 length = ntohs(*reinterpret_cast<const uint16*>(data + 2));
  if (length != data_size - kStunHeaderSize)
    return false;

  int message_type = ntohs(*reinterpret_cast<const uint16*>(data));

  // Verify that the type is known:
  switch (message_type) {
    case STUN_BINDING_REQUEST:
    case STUN_BINDING_RESPONSE:
    case STUN_BINDING_ERROR_RESPONSE:
    case STUN_SHARED_SECRET_REQUEST:
    case STUN_SHARED_SECRET_RESPONSE:
    case STUN_SHARED_SECRET_ERROR_RESPONSE:
    case STUN_ALLOCATE_REQUEST:
    case STUN_ALLOCATE_RESPONSE:
    case STUN_ALLOCATE_ERROR_RESPONSE:
    case STUN_SEND_REQUEST:
    case STUN_SEND_RESPONSE:
    case STUN_SEND_ERROR_RESPONSE:
    case STUN_DATA_INDICATION:
      *type = static_cast<StunMessageType>(message_type);
      return true;

    default:
      return false;
  }
}

// static
bool P2PSocketHost::IsRequestOrResponse(StunMessageType type) {
  return type == STUN_BINDING_REQUEST || type == STUN_BINDING_RESPONSE ||
      type == STUN_ALLOCATE_REQUEST || type == STUN_ALLOCATE_RESPONSE;
}

// static
P2PSocketHost* P2PSocketHost::Create(
    IPC::Message::Sender* message_sender, int routing_id, int id,
    P2PSocketType type) {
  switch (type) {
    case P2P_SOCKET_UDP:
      return new P2PSocketHostUdp(message_sender, routing_id, id);

    case P2P_SOCKET_TCP_SERVER:
      return new P2PSocketHostTcpServer(message_sender, routing_id, id);

    case P2P_SOCKET_TCP_CLIENT:
      return new P2PSocketHostTcp(message_sender, routing_id, id);
  }

  NOTREACHED();
  return NULL;
}

}  // namespace content
