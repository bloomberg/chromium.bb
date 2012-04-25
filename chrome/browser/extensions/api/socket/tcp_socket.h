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
  explicit TCPSocket(APIResourceEventNotifier* event_notifier);
  virtual ~TCPSocket();

  virtual int Connect(const std::string& address, int port) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual int Bind(const std::string& address, int port) OVERRIDE;
  virtual int Read(scoped_refptr<net::IOBuffer> io_buffer,
                   int io_buffer_size) OVERRIDE;
  virtual int Write(scoped_refptr<net::IOBuffer> io_buffer,
                    int bytes) OVERRIDE;
  virtual int RecvFrom(scoped_refptr<net::IOBuffer> io_buffer,
                       int io_buffer_size,
                       net::IPEndPoint *address) OVERRIDE;
  virtual int SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                     int byte_count,
                     const std::string& address,
                     int port) OVERRIDE;

  virtual void OnConnect(int result);

  static TCPSocket* CreateSocketForTesting(
      net::TCPClientSocket* tcp_client_socket,
      APIResourceEventNotifier* event_notifier);

 private:
  TCPSocket(net::TCPClientSocket* tcp_client_socket,
            APIResourceEventNotifier* event_notifier);

  scoped_ptr<net::TCPClientSocket> socket_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_TCP_SOCKET_H_
