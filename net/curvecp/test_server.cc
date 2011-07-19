// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/test_server.h"

#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/curvecp/curvecp_server_socket.h"

namespace net {

TestServer::TestServer()
    : socket_(NULL),
      errors_(0) {
}

TestServer::~TestServer() {
  if (socket_) {
    socket_->Close();
    socket_ = NULL;
  }
}

bool TestServer::Start(int port) {
  IPAddressNumber ip_number;
  std::string ip_str("0.0.0.0");
  if (!ParseIPLiteralToNumber(ip_str, &ip_number)) {
    LOG(ERROR) << "Bad IP Address";
    return false;
  }
  IPEndPoint bind_address(ip_number, port);

  DCHECK(!socket_);
  socket_ = new CurveCPServerSocket(NULL, NetLog::Source());
  int rv = socket_->Listen(bind_address, this);
  if (rv < ERR_IO_PENDING) {
    LOG(ERROR) << "Listen on port " << port << " failed: " << rv;
    return false;
  }
  return true;
}

void TestServer::RunWithParams(const Tuple1<int>& params) {
  int status = params.a;
  LOG(INFO) << "Callback! " << status;
  if (status < 0)
    MessageLoop::current()->Quit();
}

void TestServer::OnAccept(CurveCPServerSocket* new_socket) {
  DCHECK(new_socket);
  LOG(ERROR) << "Accepted socket! Starting Echo Server";
  EchoServer* new_server = new EchoServer();
  new_server->Start(new_socket);
}

EchoServer::EchoServer()
    : socket_(NULL),
      bytes_received_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &EchoServer::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &EchoServer::OnWriteComplete)) {
}

EchoServer::~EchoServer() {
}

void EchoServer::Start(CurveCPServerSocket* socket) {
  DCHECK(!socket_);
  socket_ = socket;

  ReadData();
  // Note:  |this| could be deleted here.
}

void EchoServer::OnReadComplete(int result) {
  LOG(INFO) << "Read complete: " << result;
  if (result <= 0) {
    delete this;
    return;
  }

  bytes_received_ += result;
  LOG(INFO) << "Server received " << result << "(" << bytes_received_ << ")";

  if (!received_stream_.VerifyBytes(read_buffer_->data(), result)) {
    LOG(ERROR) << "Server Received corrupt receive data!";
    delete this;
    return;
  }

  // Echo the read data back here.
  DCHECK(!write_buffer_.get());
  write_buffer_ = new DrainableIOBuffer(read_buffer_, result);
  int rv = socket_->Write(write_buffer_, result, &write_callback_);
  if (rv == ERR_IO_PENDING)
    return;
  OnWriteComplete(rv);
}

void EchoServer::OnWriteComplete(int result) {
  if (result <= 0) {
    delete this;
    return;
  }

  write_buffer_->DidConsume(result);
  while (write_buffer_->BytesRemaining()) {
    int rv = socket_->Write(write_buffer_,
                            write_buffer_->BytesRemaining(),
                            &write_callback_);
    if (rv == ERR_IO_PENDING)
      return;
    OnWriteComplete(rv);
  }

  // Now we can read more data.
  write_buffer_ = NULL;
  // read_buffer_ = NULL;
  // ReadData();
}

void EchoServer::ReadData() {
  DCHECK(!read_buffer_.get());
  read_buffer_ = new IOBuffer(kMaxMessage);

  int rv;
  do {
    rv = socket_->Read(read_buffer_, kMaxMessage, &read_callback_);
    if (rv == ERR_IO_PENDING)
      return;
    OnReadComplete(rv);  // Complete the read manually
  } while (rv > 0);
}

}  // namespace net
