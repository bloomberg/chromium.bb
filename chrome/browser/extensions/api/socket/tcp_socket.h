// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/socket/socket.h"

// This looks like it should be forward-declarable, but it does some tricky
// moves that make it easier to just include it.
#include "net/socket/tcp_client_socket.h"

namespace net {
class Socket;
}

namespace extensions {

class APIResourceEventNotifier;

class TCPSocket : public Socket {
 public:
  TCPSocket(const std::string& address, int port,
            APIResourceEventNotifier* event_notifier);
  virtual ~TCPSocket();

  virtual bool IsValid() OVERRIDE;

  virtual int Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;

  virtual void OnConnect(int result);

  static TCPSocket* CreateSocketForTesting(
      net::TCPClientSocket* tcp_client_socket,
      const std::string& address, int port,
      APIResourceEventNotifier* event_notifier);

 protected:
  virtual net::Socket* socket() OVERRIDE;

 private:
  TCPSocket(net::TCPClientSocket* tcp_client_socket,
            const std::string& address, int port,
            APIResourceEventNotifier* event_notifier);

  scoped_ptr<net::TCPClientSocket> socket_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
