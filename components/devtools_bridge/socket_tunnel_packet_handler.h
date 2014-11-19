// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_PACKET_HANDLER_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_PACKET_HANDLER_H_

#include "base/memory/ref_counted.h"

namespace net {
class IOBufferWithSize;
}

namespace devtools_bridge {

/**
 * Abstract base class for handling SocketTunnelServer/Client messages.
 */
class SocketTunnelPacketHandler {
 public:
  virtual void HandleControlPacket(int connection_index, int op_code) = 0;
  virtual void HandleDataPacket(
      int connection_index, scoped_refptr<net::IOBufferWithSize> data) = 0;
  virtual void HandleProtocolError() = 0;

  void DecodePacket(const void* data, size_t length);

 protected:
  SocketTunnelPacketHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SocketTunnelPacketHandler);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_PACKET_HANDLER_H_
