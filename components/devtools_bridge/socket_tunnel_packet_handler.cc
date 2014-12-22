// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/socket_tunnel_packet_handler.h"

#include <stdlib.h>

#include "components/devtools_bridge/socket_tunnel_connection.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace devtools_bridge {

static const int kControlConnectionId =
    SocketTunnelConnection::kControlConnectionId;
static const int kMinConnectionId = SocketTunnelConnection::kMinConnectionId;
static const int kMaxConnectionId = SocketTunnelConnection::kMaxConnectionId;
static const int kControlPacketSizeBytes =
    SocketTunnelConnection::kControlPacketSizeBytes;

void SocketTunnelPacketHandler::DecodePacket(const void* data, size_t length) {
  const unsigned char* bytes = static_cast<const unsigned char*>(data);
  if (length == 0) {
    DLOG(ERROR) << "Empty packet";
    HandleProtocolError();
    return;
  }
  int connection_id = bytes[0];
  if (connection_id != kControlConnectionId) {
    if (connection_id < kMinConnectionId ||
        connection_id > kMaxConnectionId) {
      DLOG(ERROR) << "Invalid connection ID: " << connection_id;
      HandleProtocolError();
      return;
    }

    int connection_index = connection_id - kMinConnectionId;
    scoped_refptr<net::IOBufferWithSize> packet(
        new net::IOBufferWithSize(length - 1));
    memcpy(packet->data(), bytes + 1, length - 1);
    HandleDataPacket(connection_index, packet);
  } else if (length >= kControlPacketSizeBytes) {
    static_assert(kControlPacketSizeBytes == 3,
                  "kControlPacketSizeBytes should equal 3");

    int op_code = bytes[1];
    connection_id = bytes[2];
    if (connection_id < kMinConnectionId ||
        connection_id > kMaxConnectionId) {
      DLOG(ERROR) << "Invalid connection ID: " << connection_id;
      HandleProtocolError();
      return;
    }
    int connection_index = connection_id - kMinConnectionId;
    HandleControlPacket(connection_index, op_code);
  } else {
    DLOG(ERROR) << "Invalid packet";
    HandleProtocolError();
    return;
  }
}

}  // namespace devtools_bridge
