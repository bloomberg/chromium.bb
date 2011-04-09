// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_

#include "content/common/p2p_sockets.h"

#include "ipc/ipc_message.h"
#include "net/base/ip_endpoint.h"

// Base class for P2P sockets used by P2PSocketsHost.
class P2PSocketHost {
 public:
  // Creates P2PSocketHost of the specific type.
  static P2PSocketHost* Create(IPC::Message::Sender* message_sender,
                               int routing_id, int id, P2PSocketType type);

  virtual ~P2PSocketHost();

  // Initalizes the socket. Returns false when initiazations fails.
  virtual bool Init(const net::IPEndPoint& local_address) = 0;

  // Sends |data| on the socket to |to|.
  virtual void Send(const net::IPEndPoint& to,
                    const std::vector<char>& data) = 0;

 protected:
  enum StunMessageType {
    STUN_BINDING_REQUEST = 0x0001,
    STUN_BINDING_RESPONSE = 0x0101,
    STUN_BINDING_ERROR_RESPONSE = 0x0111,
    STUN_SHARED_SECRET_REQUEST = 0x0002,
    STUN_SHARED_SECRET_RESPONSE = 0x0102,
    STUN_SHARED_SECRET_ERROR_RESPONSE = 0x0112,
    STUN_ALLOCATE_REQUEST = 0x0003,
    STUN_ALLOCATE_RESPONSE = 0x0103,
    STUN_ALLOCATE_ERROR_RESPONSE = 0x0113,
    STUN_SEND_REQUEST = 0x0004,
    STUN_SEND_RESPONSE = 0x0104,
    STUN_SEND_ERROR_RESPONSE = 0x0114,
    STUN_DATA_INDICATION = 0x0115
  };

  P2PSocketHost(IPC::Message::Sender* message_sender, int routing_id, int id);

  // Verifies that the packet |data| has a valid STUN header. In case
  // of success stores type of the message in |type|.
  bool GetStunPacketType(const char* data, int data_size,
                         StunMessageType* type);

  IPC::Message::Sender* message_sender_;
  int routing_id_;
  int id_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHost);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_
