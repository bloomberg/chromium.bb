// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_ADB_CLIENT_SOCKET_H_
#define CHROME_BROWSER_DEVTOOLS_ADB_CLIENT_SOCKET_H_

#include "base/callback.h"
#include "net/base/io_buffer.h"
#include "net/socket/tcp_client_socket.h"

class ADBClientSocket {
 public:
  typedef base::Callback<void(const std::string& error,
                              const std::string& data)> Callback;

  static void Query(int port,
                    const std::string& query,
                    const Callback& callback);

 private:
  ADBClientSocket();
  ~ADBClientSocket();

  void InnerQuery(int port, const std::string& query, const Callback& callback);
  void OnConnectComplete(scoped_refptr<net::StringIOBuffer> request_buffer,
                         int result);
  void OnWriteComplete(int result);
  void OnReadComplete(scoped_refptr<net::IOBuffer> response_buffer,
                      int result);
  bool CheckNetResultOrDie(int result);
  void ReportSuccessAndDie();
  void ReportInvalidResponseAndDie();
  void ReportErrorAndDie(const std::string& error);
  void Destroy();

  Callback callback_;
  scoped_ptr<net::TCPClientSocket> socket_;
  std::string response_;
  int expected_response_length_;

  DISALLOW_COPY_AND_ASSIGN(ADBClientSocket);
};

#endif  // CHROME_BROWSER_DEVTOOLS_ADB_CLIENT_SOCKET_H_
