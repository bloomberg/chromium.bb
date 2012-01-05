// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/udp/udp_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace extensions {

class SocketTest : public testing::Test {
};

class MockSocket : public net::UDPClientSocket {
 public:
  MockSocket()
      : net::UDPClientSocket(net::DatagramSocket::DEFAULT_BIND,
                             net::RandIntCallback(),
                             NULL,
                             net::NetLog::Source()) {}

  MOCK_METHOD3(Read, int(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback));
  MOCK_METHOD3(Write, int(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockSocket);
};

class MockSocketEventNotifier : public SocketEventNotifier {
 public:
  MockSocketEventNotifier() : SocketEventNotifier(NULL, NULL, std::string(),
                                                  0, GURL()) {}

  MOCK_METHOD2(OnReadComplete, void(int result_code,
                                    const std::string& message));
  MOCK_METHOD1(OnWriteComplete, void(int result_code));
};

TEST_F(SocketTest, TestSocketRead) {
  MockSocket* udp_client_socket = new MockSocket();
  SocketEventNotifier* notifier = new MockSocketEventNotifier();

  scoped_ptr<Socket> socket(new Socket(udp_client_socket, notifier));

  EXPECT_CALL(*udp_client_socket, Read(_, _, _))
      .Times(1);

  std::string message = socket->Read();
}

TEST_F(SocketTest, TestSocketWrite) {
  MockSocket* udp_client_socket = new MockSocket();
  SocketEventNotifier* notifier = new MockSocketEventNotifier();

  scoped_ptr<Socket> socket(new Socket(udp_client_socket, notifier));

  EXPECT_CALL(*udp_client_socket, Write(_, _, _))
      .Times(1);

  socket->Write("foo");
}

TEST_F(SocketTest, TestSocketBlockedWrite) {
  MockSocket* udp_client_socket = new MockSocket();
  MockSocketEventNotifier* notifier = new MockSocketEventNotifier();

  scoped_ptr<Socket> socket(new Socket(udp_client_socket, notifier));

  net::CompletionCallback callback;
  EXPECT_CALL(*udp_client_socket, Write(_, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(SaveArg<2>(&callback),
                               Return(net::ERR_IO_PENDING)));

  ASSERT_EQ(net::ERR_IO_PENDING, socket->Write("foo"));

  // Good. Original call came back unable to complete. Now pretend the socket
  // finished, and confirm that we passed the error back.
  EXPECT_CALL(*notifier, OnWriteComplete(42))
      .Times(1);
  callback.Run(42);
}

}  // namespace extensions
