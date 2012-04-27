// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/tcp_socket.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace extensions {

class MockTCPSocket : public net::TCPClientSocket {
 public:
  explicit MockTCPSocket(const net::AddressList& address_list)
      : net::TCPClientSocket(address_list, NULL, net::NetLog::Source()) {
  }

  MOCK_METHOD3(Read, int(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback));
  MOCK_METHOD3(Write, int(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback));
  virtual bool IsConnected() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTCPSocket);
};

class MockAPIResourceEventNotifier : public APIResourceEventNotifier {
 public:
  MockAPIResourceEventNotifier() : APIResourceEventNotifier(NULL, NULL,
                                                            std::string(),
                                                            0, GURL()) {}

  MOCK_METHOD2(OnReadComplete, void(int result_code,
                                    const std::string& message));
  MOCK_METHOD1(OnWriteComplete, void(int result_code));

 protected:
  virtual ~MockAPIResourceEventNotifier() {}
};

TEST(SocketTest, TestTCPSocketRead) {
  net::AddressList address_list;
  MockTCPSocket* tcp_client_socket = new MockTCPSocket(address_list);
  APIResourceEventNotifier* notifier = new MockAPIResourceEventNotifier();

  scoped_ptr<TCPSocket> socket(TCPSocket::CreateSocketForTesting(
      tcp_client_socket, notifier));

  EXPECT_CALL(*tcp_client_socket, Read(_, _, _))
      .Times(1);

  scoped_refptr<net::IOBufferWithSize> io_buffer(
      new net::IOBufferWithSize(512));
  socket->Read(io_buffer.get(), io_buffer->size());
}

TEST(SocketTest, TestTCPSocketWrite) {
  net::AddressList address_list;
  MockTCPSocket* tcp_client_socket = new MockTCPSocket(address_list);
  APIResourceEventNotifier* notifier = new MockAPIResourceEventNotifier();

  scoped_ptr<TCPSocket> socket(TCPSocket::CreateSocketForTesting(
      tcp_client_socket, notifier));

  EXPECT_CALL(*tcp_client_socket, Write(_, _, _))
      .Times(1);

  scoped_refptr<net::IOBufferWithSize> io_buffer(
      new net::IOBufferWithSize(256));
  socket->Write(io_buffer.get(), io_buffer->size());
}

TEST(SocketTest, TestTCPSocketBlockedWrite) {
  net::AddressList address_list;
  MockTCPSocket* tcp_client_socket = new MockTCPSocket(address_list);
  MockAPIResourceEventNotifier* notifier = new MockAPIResourceEventNotifier();

  scoped_ptr<TCPSocket> socket(TCPSocket::CreateSocketForTesting(
      tcp_client_socket, notifier));

  net::CompletionCallback callback;
  EXPECT_CALL(*tcp_client_socket, Write(_, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(SaveArg<2>(&callback),
                               Return(net::ERR_IO_PENDING)));

  scoped_refptr<net::IOBufferWithSize> io_buffer(new net::IOBufferWithSize(
      1));
  ASSERT_EQ(net::ERR_IO_PENDING, socket->Write(io_buffer.get(),
                                               io_buffer->size()));

  // Good. Original call came back unable to complete. Now pretend the socket
  // finished, and confirm that we passed the error back.
  EXPECT_CALL(*notifier, OnWriteComplete(42))
      .Times(1);
  callback.Run(42);
}

}  // namespace extensions
