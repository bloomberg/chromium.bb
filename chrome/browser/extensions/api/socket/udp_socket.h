// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/socket/socket.h"
#include "net/udp/udp_socket.h"

namespace net {
class Socket;
}

namespace extensions {

class APIResourceEventNotifier;

class UDPSocket : public Socket {
 public:
  explicit UDPSocket(APIResourceEventNotifier* event_notifier);
  virtual ~UDPSocket();

  virtual int Connect(const std::string& address, int port) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual int Bind(const std::string& address, int port) OVERRIDE;
  virtual int Read(scoped_refptr<net::IOBuffer> io_buffer,
                   int io_buffer_size) OVERRIDE;
  virtual int Write(scoped_refptr<net::IOBuffer> io_buffer,
                    int bytes) OVERRIDE;
  virtual int RecvFrom(scoped_refptr<net::IOBuffer> io_buffer,
                       int io_buffer_size,
                       net::IPEndPoint* address) OVERRIDE;
  virtual int SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                     int byte_count,
                     const std::string& address,
                     int port) OVERRIDE;

 private:
  scoped_ptr<net::UDPSocket> socket_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
