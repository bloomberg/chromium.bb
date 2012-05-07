// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/socket/socket.h"
#include "net/udp/udp_socket.h"

namespace extensions {

class APIResourceEventNotifier;

class UDPSocket : public Socket {
 public:
  explicit UDPSocket(APIResourceEventNotifier* event_notifier);
  virtual ~UDPSocket();

  virtual void Connect(const std::string& address,
                       int port,
                       const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual int Bind(const std::string& address, int port) OVERRIDE;
  virtual void Read(int count,
                    const ReadCompletionCallback& callback) OVERRIDE;
  virtual void Write(scoped_refptr<net::IOBuffer> io_buffer,
                     int bytes,
                     const CompletionCallback& callback) OVERRIDE;
  virtual void RecvFrom(int count,
                        const RecvFromCompletionCallback& callback) OVERRIDE;
  virtual void SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                      int byte_count,
                      const std::string& address,
                      int port,
                      const CompletionCallback& callback) OVERRIDE;

 private:
  // Make net::IPEndPoint can be refcounted
  typedef base::RefCountedData<net::IPEndPoint> IPEndPoint;

  virtual void OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer,
                              int result);
  virtual void OnWriteComplete(int result);
  virtual void OnRecvFromComplete(scoped_refptr<net::IOBuffer> io_buffer,
                                  scoped_refptr<IPEndPoint> address,
                                  int result);
  virtual void OnSendToComplete(int result);

  net::UDPSocket socket_;

  ReadCompletionCallback read_callback_;

  CompletionCallback write_callback_;

  RecvFromCompletionCallback recv_from_callback_;

  CompletionCallback send_to_callback_;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_UDP_SOCKET_H_
