// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/socket/socket.h"
#include "net/udp/datagram_client_socket.h"

namespace net {
class Socket;
}

namespace extensions {

class APIResourceEventNotifier;

class UDPSocket : public Socket {
 public:
  UDPSocket(const std::string& address, int port,
            APIResourceEventNotifier* event_notifier);
  virtual ~UDPSocket();

  virtual bool IsValid() OVERRIDE;

  virtual int Connect() OVERRIDE;
  virtual void Disconnect() OVERRIDE;

  static UDPSocket* CreateSocketForTesting(
      net::DatagramClientSocket* datagram_client_socket,
      const std::string& address, int port,
      APIResourceEventNotifier* event_notifier);

 protected:
  virtual net::Socket* socket() OVERRIDE;

 private:
  // Special constructor for testing.
  UDPSocket(net::DatagramClientSocket* datagram_client_socket,
            const std::string& address, int port,
            APIResourceEventNotifier* event_notifier);

  scoped_ptr<net::DatagramClientSocket> socket_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
