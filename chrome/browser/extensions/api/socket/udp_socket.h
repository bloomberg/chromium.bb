// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"

// This looks like it should be forward-declarable, but it does some tricky
// moves that make it easier to just include it.
#include "net/udp/datagram_client_socket.h"

namespace net {
class Socket;
}

namespace extensions {

class UDPSocket : public Socket {
 public:
  UDPSocket(net::DatagramClientSocket* datagram_client_socket,
            const std::string& address, int port,
            SocketEventNotifier* event_notifier);
  virtual ~UDPSocket();

  virtual int Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;

 protected:
  virtual net::Socket* socket() OVERRIDE;

 private:
  scoped_ptr<net::DatagramClientSocket> socket_;
  const std::string address_;
  int port_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
