// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_

#include "content/common/content_export.h"
#include "content/common/p2p_sockets.h"

#include "ipc/ipc_message.h"
#include "net/base/ip_endpoint.h"

namespace content {

// Base class for P2P sockets.
class CONTENT_EXPORT P2PSocketHost {
 public:
  // Creates P2PSocketHost of the specific type.
  static P2PSocketHost* Create(IPC::Sender* message_sender,
                               int id, P2PSocketType type);

  virtual ~P2PSocketHost();

  // Initalizes the socket. Returns false when initiazations fails.
  virtual bool Init(const net::IPEndPoint& local_address,
                    const net::IPEndPoint& remote_address) = 0;

  // Sends |data| on the socket to |to|.
  virtual void Send(const net::IPEndPoint& to,
                    const std::vector<char>& data) = 0;

  virtual P2PSocketHost* AcceptIncomingTcpConnection(
      const net::IPEndPoint& remote_address, int id) = 0;

 protected:
  friend class P2PSocketHostTcpTest;

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

  enum State {
    STATE_UNINITIALIZED,
    STATE_CONNECTING,
    STATE_OPEN,
    STATE_ERROR,
  };

  // Maximum size of send buffers. Must be big enough to fit data for
  // one data burst. Send buffers size needs to be limited to prevent
  // from consuming too much memory with misbehaving renderer process.
  //
  // TODO(sergeyu): Consider implementing congestion notifications to
  // minimize buffering. This will require some fixes in libjingle,
  // see crbug.com/91495 .
  static const int kMaxSendBufferSize = 256 * 1024;

  P2PSocketHost(IPC::Sender* message_sender, int id);

  // Verifies that the packet |data| has a valid STUN header. In case
  // of success stores type of the message in |type|.
  static bool GetStunPacketType(const char* data, int data_size,
                                StunMessageType* type);
  static bool IsRequestOrResponse(StunMessageType type);

  IPC::Sender* message_sender_;
  int id_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(P2PSocketHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_HOST_H_
