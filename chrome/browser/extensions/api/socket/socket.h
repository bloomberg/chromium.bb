// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "net/base/io_buffer.h"
#include "net/udp/datagram_client_socket.h"

namespace net {
class IPEndPoint;
}

namespace extensions {

// A Socket wraps a low-level socket and includes housekeeping information that
// we need to manage it in the context of an extension.
class Socket {
 public:
  Socket(net::DatagramClientSocket* datagram_client_socket,
         SocketEventNotifier* event_notifier);
  virtual ~Socket();

  // Returns true if successful.
  virtual bool Connect(const net::IPEndPoint& ip_end_point);
  virtual void Close();

  virtual std::string Read();

  // Returns the number of bytes successfully written, or a negative error
  // code. Note that ERR_IO_PENDING means that the operation blocked, in which
  // case |event_notifier| will eventually be called with the final result
  // (again, either a nonnegative number of bytes written, or a negative
  // error).
  virtual int Write(const std::string message);

 private:
  scoped_ptr<net::DatagramClientSocket> datagram_client_socket_;
  scoped_ptr<SocketEventNotifier> event_notifier_;
  bool is_connected_;
  static const int kMaxRead = 1024;
  scoped_refptr<net::IOBufferWithSize> read_buffer_;

  void OnReadComplete(int result);
  void OnWriteComplete(int result);
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_H_
