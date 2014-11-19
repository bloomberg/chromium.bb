// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_CONNECTION_H_
#define COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_CONNECTION_H_

#include <deque>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class IOBufferWithSize;
class StreamSocket;
}

namespace devtools_bridge {

/**
 * Abstract base class for SocketTunnelServer/Client connection.
 *
 * Connection binds a pair of net::StreamSocket (or alike) through
 * a data channel. SocketTunnel may handle up to kMaxConnectionCount
 * simultaneous connection (DevTools can keep ~10 connection;
 * other connections hang in unopened state; additional connections
 * could help to deal with data channel latency).
 *
 * Client should create net::StreamListenSocket (or logical equivalent)
 * and listen for incoming connection. When one comes it sends CLIENT_OPEN
 * packet to the server.
 *
 * Server transforms client's packet to calls of net::StreamSocket. On
 * CLIENT_OPEN it creates a socket and connects. If connection succeeds
 * it sends back SERVER_OPEN_ACK. If it fails it sends SERVER_CLOSE.
 *
 * After SERVER_OPEN_ACK server may send SERVER_CLOSE any time (if the socket
 * it connects to has closed on another side). If client closes the connection
 * sending CLIENT_CLOSE server acknowledges it by sending SERVER_CLOSE.
 * Client may reuse connection ID once it received SERVER_CLOSE (because
 * data channel is ordered and reliable).
 */
class SocketTunnelConnection {
 public:
  enum ClientOpCode {
    CLIENT_OPEN = 0,
    CLIENT_CLOSE = 1
  };

  enum ServerOpCode {
    SERVER_OPEN_ACK = 0,
    SERVER_CLOSE = 1
  };

  static const int kMaxConnectionCount = 64;

  static const int kMaxPacketSizeBytes = 1024 * 4;
  static const int kControlPacketSizeBytes = 3;

  static const int kControlConnectionId = 0;

  static const int kMinConnectionId = 1;
  static const int kMaxConnectionId =
      kMinConnectionId + kMaxConnectionCount - 1;

  void Write(scoped_refptr<net::IOBufferWithSize> chunk);
  void ReadNextChunk();

 protected:
  SocketTunnelConnection(int index);
  ~SocketTunnelConnection();

  const int index_;

  // |buffer| length must be kControlPacketSizeBytes.
  void BuildControlPacket(char* buffer, int op_code);

  virtual net::StreamSocket* socket() = 0;
  virtual void OnDataPacketRead(const void* data, size_t length) = 0;
  virtual void OnReadError(int error) = 0;

 private:
  void WriteCurrent();
  void OnWriteComplete(int result);
  void OnReadComplete(int result);

  std::deque<scoped_refptr<net::IOBufferWithSize> > buffer_;
  scoped_refptr<net::DrainableIOBuffer> current_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SocketTunnelConnection);
};

}  // namespace devtools_bridge

#endif  // COMPONENTS_DEVTOOLS_BRIDGE_SOCKET_TUNNEL_CONNECTION_H_
