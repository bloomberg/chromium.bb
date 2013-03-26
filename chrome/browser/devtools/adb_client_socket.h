// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_ADB_CLIENT_SOCKET_H_
#define CHROME_BROWSER_DEVTOOLS_ADB_CLIENT_SOCKET_H_

#include "base/callback.h"
#include "net/base/io_buffer.h"
#include "net/socket/tcp_client_socket.h"

class AdbClientSocket {
 public:
  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result,
                              net::TCPClientSocket*)> SocketCallback;

  static void AdbQuery(int port,
                       const std::string& query,
                       const CommandCallback& callback);

  static void HttpQuery(int port,
                        const std::string& serial,
                        const std::string& socket_name,
                        const std::string& request,
                        const CommandCallback& callback);

  static void HttpQuery(int port,
                        const std::string& serial,
                        const std::string& socket_name,
                        const std::string& request,
                        const SocketCallback& callback);

  AdbClientSocket(const std::string& host, int port);
  ~AdbClientSocket();

 protected:
  void Connect(const net::CompletionCallback& callback);

  void SendCommand(const std::string& command,
                   bool is_void,
                   const CommandCallback& callback);

  scoped_ptr<net::TCPClientSocket> socket_;

 private:
  void ReadResponse(const CommandCallback& callback, bool is_void, int result);

  void OnResponseHeader(const CommandCallback& callback,
                        bool is_void,
                        scoped_refptr<net::IOBuffer> response_buffer,
                        int result);

  void OnResponseData(const CommandCallback& callback,
                      const std::string& response,
                      scoped_refptr<net::IOBuffer> response_buffer,
                      int bytes_left,
                      int result);

  std::string host_;
  int port_;

  DISALLOW_COPY_AND_ASSIGN(AdbClientSocket);
};

#endif  // CHROME_BROWSER_DEVTOOLS_ADB_CLIENT_SOCKET_H_
