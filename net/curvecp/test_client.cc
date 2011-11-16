// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/test_client.h"

#include <string>
#include <deque>
#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/single_request_host_resolver.h"
#include "net/curvecp/curvecp_client_socket.h"

namespace net {

TestClient::TestClient()
    : socket_(NULL),
      errors_(0),
      bytes_to_send_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &TestClient::OnConnectComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &TestClient::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &TestClient::OnWriteComplete)),
      finished_callback_(NULL) {
}

TestClient::~TestClient() {
  if (socket_) {
    // TODO(mbelshe): The CurveCPClientSocket has a method called Disconnect.
    //                The CurveCPServerSocket has a method called Close.
    //                Unify them into either Close or Disconnect!!!
    socket_->Disconnect();
    socket_ = NULL;
  }
}

bool TestClient::Start(const HostPortPair& server_host_port_pair,
                       int bytes_to_send,
                       OldCompletionCallback* callback) {
  DCHECK(!socket_);
  DCHECK(!finished_callback_);

  finished_callback_ = callback;
  bytes_to_read_ = bytes_to_send_ = bytes_to_send;

  scoped_ptr<HostResolver> system_host_resolver(
      CreateSystemHostResolver(1, 0, NULL));
  SingleRequestHostResolver host_resolver(system_host_resolver.get());
  HostResolver::RequestInfo request(server_host_port_pair);
  AddressList addresses;
  int rv = host_resolver.Resolve(request, &addresses, CompletionCallback(),
                                 BoundNetLog());
  if (rv != OK) {
    LOG(ERROR) << "Could not resolve host";
    return false;
  }

  socket_ = new CurveCPClientSocket(addresses, NULL, NetLog::Source());
  rv = socket_->Connect(&connect_callback_);
  if (rv == ERR_IO_PENDING)
    return true;
  OnConnectComplete(rv);
  return rv == OK;
}

void TestClient::OnConnectComplete(int result) {
  LOG(ERROR) << "Connect complete";
  if (result < 0) {
    LOG(ERROR) << "Connect failure: " << result;
    errors_++;
    Finish(result);
    return;
  }

  DCHECK(bytes_to_send_);   // We should have data to send.
  ReadData();

  DCHECK_EQ(bytes_to_send_, bytes_to_read_);

  SendData();
}

void TestClient::OnReadComplete(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Read failure: " << result;
    errors_++;
    Finish(result);
    return;
  }

  if (!received_stream_.VerifyBytes(read_buffer_->data(), result)) {
    if (!errors_)  // Only log the first error.
      LOG(ERROR) << "Client Received corrupt receive data!";
    errors_++;
  }

  read_buffer_ = NULL;
  bytes_to_read_ -= result;

  // Now read more data...
  if (bytes_to_read_)
    ReadData();
  else
    Finish(OK);
}

void TestClient::OnWriteComplete(int result) {
  LOG(ERROR) << "Write complete (remaining = " << bytes_to_send_ << ")";
  if (result <= 0) {
    errors_++;
    Finish(result);
    return;
  }

  write_buffer_->DidConsume(result);
  bytes_to_send_ -= result;
  if (!write_buffer_->BytesRemaining())
      write_buffer_ = NULL;

  if (bytes_to_send_)
    SendData();
}

void TestClient::ReadData() {
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

void TestClient::SendData() {
  DCHECK(bytes_to_send_);   // We should have data to send.
  const int kWriteChunkSize = 777;  // 777 is more abusive

  do {
    if (!write_buffer_.get()) {
      int bytes_to_send = std::min(kWriteChunkSize, bytes_to_send_);
      scoped_refptr<IOBuffer> buffer(new IOBuffer(bytes_to_send));
      sent_stream_.GetBytes(buffer->data(), bytes_to_send);
      write_buffer_ = new DrainableIOBuffer(buffer, bytes_to_send);
    }

    int rv = socket_->Write(write_buffer_,
                            write_buffer_->BytesRemaining(),
                            &write_callback_);
    if (rv == ERR_IO_PENDING)
      return;

    write_buffer_->DidConsume(rv);
    bytes_to_send_ -= rv;
    if (!write_buffer_->BytesRemaining())
      write_buffer_ = NULL;
  } while (bytes_to_send_);
}

void TestClient::Finish(int result) {
  DCHECK(finished_callback_);

  LOG(ERROR) << "TestClient Done!";
  OldCompletionCallback* callback = finished_callback_;
  finished_callback_ = NULL;
  callback->Run(result);
}

}  // namespace net
