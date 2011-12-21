// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEB_SOCKET_SERVER_SOCKET_H_
#define NET_SOCKET_WEB_SOCKET_SERVER_SOCKET_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"
#include "net/socket/socket.h"

namespace net {

// WebSocketServerSocket takes an (already connected) underlying transport
// socket and speaks server-side websocket protocol atop of it.
// This class implements Socket interface: notice that Read() returns (or calls
// back) as soon as some amount of data is available, even if message/frame is
// incomplete.
class WebSocketServerSocket : public Socket {
 public:
  class Delegate {
   public:
    // Validates websocket handshake: return false to reject handshake.
    // |resource| is name of resource requested in GET stanza of handshake;
    // |origin| is origin as reported in handshake;
    // |host| is Host field from handshake;
    // |subprotocol_list| is derived from Sec-WebSocket-Protocol field.
    // Output parameters are:
    // |location_out| is location of websocket server;
    // |subprotocol_out| is selected subprotocol (or empty string if subprotocol
    // list is empty.
    virtual bool ValidateWebSocket(
        const std::string& resource,
        const std::string& origin,
        const std::string& host,
        const std::vector<std::string>& subprotocol_list,
        std::string* location_out,
        std::string* subprotocol_out) = 0;

    virtual ~Delegate() {}
  };

  virtual ~WebSocketServerSocket();

  // Performs websocket server handshake on transport socket. Underlying socket
  // must have already been connected/accepted.
  //
  // Returns either ERR_IO_PENDING, in which case the given callback will be
  // called in the future with the real result, or it completes synchronously,
  // returning the result immediately.
  virtual int Accept(const CompletionCallback& callback) = 0;
};

// Creates websocket server socket atop of already connected socket. This
// created server socket will take ownership of |transport_socket|.
NET_EXPORT WebSocketServerSocket* CreateWebSocketServerSocket(
    Socket* transport_socket,
    WebSocketServerSocket::Delegate* delegate);

}  // namespace net

#endif  // NET_SOCKET_WEB_SOCKET_SERVER_SOCKET_H_
