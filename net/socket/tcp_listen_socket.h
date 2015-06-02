// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_LISTEN_SOCKET_H_
#define NET_SOCKET_TCP_LISTEN_SOCKET_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/net_export.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/stream_listen_socket.h"

namespace net {

namespace test_server {
class EmbeddedTestServer;
}

// Implements a TCP socket. This class is deprecated and will be removed
// once crbug.com/472766 is fixed. There should not be any new consumer of this
// class.
class NET_EXPORT TCPListenSocket : public StreamListenSocket {
 public:
  ~TCPListenSocket() override;

 protected:
  TCPListenSocket(SocketDescriptor s, StreamListenSocket::Delegate* del);

  // Implements StreamListenSocket::Accept.
  void Accept() override;

 private:
  // Note that friend classes are temporary until crbug.com/472766 is fixed.
  friend class test_server::EmbeddedTestServer;
  friend class TCPListenSocketTester;

  // Listen on port for the specified IP address.  Use 127.0.0.1 to only
  // accept local connections.
  static scoped_ptr<TCPListenSocket> CreateAndListen(
      const std::string& ip,
      uint16 port,
      StreamListenSocket::Delegate* del);

  // Get raw TCP socket descriptor bound to ip:port.
  static SocketDescriptor CreateAndBind(const std::string& ip, uint16 port);

  // Get raw TCP socket descriptor bound to ip and return port it is bound to.
  static SocketDescriptor CreateAndBindAnyPort(const std::string& ip,
                                               uint16* port);

  DISALLOW_COPY_AND_ASSIGN(TCPListenSocket);
};

}  // namespace net

#endif  // NET_SOCKET_TCP_LISTEN_SOCKET_H_
