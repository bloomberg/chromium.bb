// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_ADB_CLIENT_SOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_NET_ADB_CLIENT_SOCKET_H_

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

class AdbClientSocket {
 public:
  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result,
                              net::StreamSocket*)> SocketCallback;

  static void AdbQuery(int port,
                       const std::string& query,
                       const CommandCallback& callback);

  static void TransportQuery(int port,
                             const std::string& serial,
                             const std::string& socket_name,
                             const SocketCallback& callback);

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

  static void SendFile(int port,
                       const std::string& serial,
                       const std::string& filename,
                       const std::string& content,
                       const CommandCallback& callback);

  explicit AdbClientSocket(int port);
  ~AdbClientSocket();

 protected:
  void Connect(const net::CompletionCallback& callback);

  void SendCommand(const std::string& command,
                   bool is_void,
                   bool has_length,
                   const CommandCallback& callback);

  std::unique_ptr<net::StreamSocket> socket_;

  void ReadResponse(const CommandCallback& callback,
                    bool is_void,
                    bool has_length,
                    int result);

 private:
  void OnResponseStatus(const CommandCallback& callback,
                        bool is_void,
                        bool has_length,
                        scoped_refptr<net::IOBuffer> response_buffer,
                        int result);

  void OnResponseLength(const CommandCallback& callback,
                        const std::string& response,
                        scoped_refptr<net::IOBuffer> response_buffer,
                        int result);

  void OnResponseData(const CommandCallback& callback,
                      const std::string& response,
                      scoped_refptr<net::IOBuffer> response_buffer,
                      int bytes_left,
                      int result);

  int port_;

  DISALLOW_COPY_AND_ASSIGN(AdbClientSocket);
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_ADB_CLIENT_SOCKET_H_
