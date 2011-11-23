// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_TEST_SERVER_H_
#define NET_CURVECP_TEST_SERVER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/test_data_stream.h"
#include "net/curvecp/curvecp_server_socket.h"

namespace net {

class DrainableIOBuffer;
class EchoServer;
class IOBuffer;

// TestServer is the server which processes the listen socket.
// It will create an EchoServer instance to handle each connection.
class TestServer : public OldCompletionCallback,
                   public CurveCPServerSocket::Acceptor {
 public:
  TestServer();
  virtual ~TestServer();

  bool Start(int port);

  // OldCompletionCallback methods:
  virtual void RunWithParams(const Tuple1<int>& params) OVERRIDE;

  // CurveCPServerSocket::Acceptor methods:
  virtual void OnAccept(CurveCPServerSocket* new_socket) OVERRIDE;

  // Returns the number of errors this server encountered.
  int error_count() { return errors_; }

 private:
  CurveCPServerSocket* socket_;
  int errors_;
};


// EchoServer does the actual server work for a connection.
// This object self destructs after finishing its work.
class EchoServer {
 public:
  EchoServer();
  ~EchoServer();

  // Start the Echo Server
  void Start(CurveCPServerSocket* socket);

 private:
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  void ReadData();

 private:
  static const int kMaxMessage = 1024;
  CurveCPServerSocket* socket_;
  scoped_refptr<IOBuffer> read_buffer_;
  scoped_refptr<DrainableIOBuffer> write_buffer_;
  TestDataStream received_stream_;
  int bytes_received_;
  OldCompletionCallbackImpl<EchoServer> read_callback_;
  OldCompletionCallbackImpl<EchoServer> write_callback_;
};

}  // namespace net

#endif  // NET_CURVECP_TEST_SERVER_H_
