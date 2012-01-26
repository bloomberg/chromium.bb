// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"

// This looks like it should be forward-declarable, but it does some tricky
// moves that make it easier to just include it.
#include "net/socket/tcp_client_socket.h"

namespace net {
class Socket;
}

namespace extensions {

class TCPSocket : public Socket {
 public:
  TCPSocket(net::TCPClientSocket* tcp_client_socket,
            SocketEventNotifier* event_notifier);
  virtual ~TCPSocket();

  virtual int Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;

  virtual void OnConnect(int result);

 protected:
  virtual net::Socket* socket() OVERRIDE;

 private:
  scoped_ptr<net::TCPClientSocket> socket_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
