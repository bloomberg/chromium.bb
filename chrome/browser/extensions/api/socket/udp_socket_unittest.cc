// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/udp_socket.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/udp/udp_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace extensions {

class MockUDPSocket : public net::UDPClientSocket {
 public:
  MockUDPSocket()
      : net::UDPClientSocket(net::DatagramSocket::DEFAULT_BIND,
                             net::RandIntCallback(),
                             NULL,
                             net::NetLog::Source()) {}

  MOCK_METHOD3(Read, int(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback));
  MOCK_METHOD3(Write, int(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockUDPSocket);
};

class MockAPIResourceEventNotifier : public APIResourceEventNotifier {
 public:
  MockAPIResourceEventNotifier() : APIResourceEventNotifier(NULL, NULL,
                                                            std::string(),
                                                            0, GURL()) {}

  MOCK_METHOD2(OnReadComplete, void(int result_code,
                                    const std::string& message));
  MOCK_METHOD1(OnWriteComplete, void(int result_code));
};

TEST(SocketTest, TestUDPSocketRead) {
  MockUDPSocket* udp_client_socket = new MockUDPSocket();
  APIResourceEventNotifier* notifier = new MockAPIResourceEventNotifier();

  scoped_ptr<UDPSocket> socket(UDPSocket::CreateSocketForTesting(
      udp_client_socket, "1.2.3.4", 1, notifier));

  EXPECT_CALL(*udp_client_socket, Read(_, _, _))
      .Times(1);

  scoped_refptr<net::IOBufferWithSize> io_buffer(
      new net::IOBufferWithSize(512));
  socket->Read(io_buffer.get(), io_buffer->size());
}

TEST(SocketTest, TestUDPSocketWrite) {
  MockUDPSocket* udp_client_socket = new MockUDPSocket();
  APIResourceEventNotifier* notifier = new MockAPIResourceEventNotifier();

  scoped_ptr<UDPSocket> socket(UDPSocket::CreateSocketForTesting(
      udp_client_socket, "1.2.3.4", 1, notifier));

  EXPECT_CALL(*udp_client_socket, Write(_, _, _))
      .Times(1);

  scoped_refptr<net::IOBufferWithSize> io_buffer(
      new net::IOBufferWithSize(512));
  socket->Write(io_buffer.get(), io_buffer->size());
}

TEST(SocketTest, TestUDPSocketBlockedWrite) {
  MockUDPSocket* udp_client_socket = new MockUDPSocket();
  MockAPIResourceEventNotifier* notifier = new MockAPIResourceEventNotifier();

  scoped_ptr<UDPSocket> socket(UDPSocket::CreateSocketForTesting(
      udp_client_socket, "1.2.3.4", 1, notifier));

  net::CompletionCallback callback;
  EXPECT_CALL(*udp_client_socket, Write(_, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(SaveArg<2>(&callback),
                               Return(net::ERR_IO_PENDING)));

  scoped_refptr<net::IOBufferWithSize> io_buffer(new net::IOBufferWithSize(1));
  ASSERT_EQ(net::ERR_IO_PENDING, socket->Write(io_buffer.get(),
                                               io_buffer->size()));

  // Good. Original call came back unable to complete. Now pretend the socket
  // finished, and confirm that we passed the error back.
  EXPECT_CALL(*notifier, OnWriteComplete(42))
      .Times(1);
  callback.Run(42);
}

}  // namespace extensions
