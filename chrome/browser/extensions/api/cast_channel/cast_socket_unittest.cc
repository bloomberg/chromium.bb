// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"

#include "chrome/browser/extensions/api/cast_channel/cast_channel.pb.h"
#include "net/base/address_list.h"
#include "net/base/capturing_net_log.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::A;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

namespace extensions {
namespace api {
namespace cast_channel {

class MockCastSocketDelegate : public CastSocket::Delegate {
 public:
  MOCK_METHOD2(OnError, void(const CastSocket* socket,
                             ChannelError error));
  MOCK_METHOD3(OnStringMessage, void(const CastSocket* socket,
                                     const std::string& namespace_,
                                     const std::string& data));
};

class MockTCPClientSocket : public net::TCPClientSocket {
 public:
  explicit MockTCPClientSocket(const net::AddressList& addresses) :
    TCPClientSocket(addresses, NULL, net::NetLog::Source()) { }
  virtual ~MockTCPClientSocket() { }

  MOCK_METHOD1(Connect, int(const net::CompletionCallback& callback));
  MOCK_METHOD2(SetKeepAlive, bool(bool, int));
  MOCK_METHOD1(SetNoDelay, bool(bool));
  MOCK_METHOD3(Read, int(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback));
  MOCK_METHOD3(Write, int(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback));
  MOCK_METHOD0(Disconnect, void());
};

class CompleteHandler {
 public:
  CompleteHandler() {}
  MOCK_METHOD1(OnCloseComplete, void(int result));
  MOCK_METHOD1(OnConnectComplete, void(int result));
  MOCK_METHOD1(OnWriteComplete, void(int result));
 private:
  DISALLOW_COPY_AND_ASSIGN(CompleteHandler);
};

class TestCastSocket : public CastSocket {
 public:
  explicit TestCastSocket(MockCastSocketDelegate* delegate) :
    CastSocket("abcdefg", GURL("cast://192.0.0.1:8009"), delegate,
               &capturing_net_log_), owns_socket_(true) {
    net::AddressList addresses;
    mock_tcp_socket_ = new MockTCPClientSocket(addresses);
  }

  virtual ~TestCastSocket() {
    if (owns_socket_) {
      DCHECK(mock_tcp_socket_);
      delete mock_tcp_socket_;
    }
  }

  virtual void Close(const net::CompletionCallback& callback) OVERRIDE {
    if (!owns_socket_)
      mock_tcp_socket_ = NULL;
    CastSocket::Close(callback);
  }

  // Ptr to the mock socket.  Ownership is transferred to CastSocket when it is
  // returned from CreateSocket().  CastSocket will destroy it on Close(),
  // so don't refer to |mock_tcp_socket_| afterwards.
  MockTCPClientSocket* mock_tcp_socket_;

 protected:
  // Transfers ownership of |mock_tcp_socket_| to CastSocket.
  virtual net::TCPClientSocket* CreateSocket(
      const net::AddressList& addresses,
      net::NetLog* net_log,
      const net::NetLog::Source& source) OVERRIDE {
    owns_socket_ = false;
    return mock_tcp_socket_;
  }

 private:
  net::CapturingNetLog capturing_net_log_;
  // Whether this object or the parent owns |mock_tcp_socket_|.
  bool owns_socket_;
};

class CastSocketTest : public testing::Test {
 public:
  CastSocketTest() {}
  virtual ~CastSocketTest() {}

  virtual void SetUp() OVERRIDE {
    socket_.reset(new TestCastSocket(&mock_delegate_));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(handler_, OnCloseComplete(net::OK));
    socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                              base::Unretained(&handler_)));
  }

  // Sets expectations when the socket is connected.  Connecting the socket also
  // starts the read loop; we expect the call to Read(), but never fire the read
  // callback.
  void ConnectHelper() {
    net::CompletionCallback connect_callback;
    EXPECT_CALL(mock_tcp_socket(), SetNoDelay(true))
      .Times(1)
      .WillOnce(Return(true));
    EXPECT_CALL(mock_tcp_socket(), SetKeepAlive(true, A<int>()))
      .Times(1)
      .WillOnce(Return(true));
    EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback), Return(net::OK)));
    EXPECT_CALL(handler_, OnConnectComplete(net::OK));
    EXPECT_CALL(mock_tcp_socket(), Read(A<net::IOBuffer*>(),
                                        A<int>(),
                                        A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(Return(net::ERR_IO_PENDING));

    socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                                base::Unretained(&handler_)));
    connect_callback.Run(net::OK);
    EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
    EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
  }

 protected:
  MockTCPClientSocket& mock_tcp_socket() {
    MockTCPClientSocket* mock_socket = socket_->mock_tcp_socket_;
    DCHECK(mock_socket);
    return *mock_socket;
  }

  MockCastSocketDelegate mock_delegate_;
  scoped_ptr<TestCastSocket> socket_;
  CompleteHandler handler_;
};

// Tests URL parsing and validation.
TEST_F(CastSocketTest, TestCastURLs) {
  EXPECT_TRUE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:8009")));
  EXPECT_FALSE(socket_->is_secure_);
  EXPECT_EQ(socket_->ip_endpoint_.ToString(), "192.0.0.1:8009");

  EXPECT_TRUE(socket_->ParseChannelUrl(GURL("casts://192.0.0.1:12345")));
  EXPECT_TRUE(socket_->is_secure_);
  EXPECT_EQ(socket_->ip_endpoint_.ToString(), "192.0.0.1:12345");

  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("http://192.0.0.1:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast:192.0.0.1:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast:///192.0.0.1:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://:12345")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://abcd:8009")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:abcd")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("foo")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast:")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast::")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://:")));
  EXPECT_FALSE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:")));
}

// Tests connecting and closing the socket.
TEST_F(CastSocketTest, TestConnectAndClose) {
  ConnectHelper();

  EXPECT_CALL(handler_, OnCloseComplete(net::OK));
  socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                            base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests writing a single message where the completion is signaled via callback.
TEST_F(CastSocketTest, TestWriteViaCallback) {
  ConnectHelper();
  net::CompletionCallback write_callback;

  EXPECT_CALL(mock_tcp_socket(),
              Write(A<net::IOBuffer*>(),
                    33,
                    A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<2>(&write_callback),
                    Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(handler_, OnWriteComplete(33));
  socket_->SendString("urn:test", "Hello, World!",
                      base::Bind(&CompleteHandler::OnWriteComplete,
                                 base::Unretained(&handler_)));
  write_callback.Run(33);
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests writing a single message where the Write() returns directly.
TEST_F(CastSocketTest, TestWrite) {
  ConnectHelper();

  EXPECT_CALL(mock_tcp_socket(),
              Write(A<net::IOBuffer*>(),
                    33,
                    A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(Return(33));
  EXPECT_CALL(handler_, OnWriteComplete(33));
  socket_->SendString("urn:test", "Hello, World!",
                      base::Bind(&CompleteHandler::OnWriteComplete,
                                 base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests writing multiple messages.
TEST_F(CastSocketTest, TestWriteMany) {
  ConnectHelper();
  std::string messages[4];
  messages[0] = "Hello, World!";
  messages[1] = "Goodbye, World!";
  messages[2] = "Hello, Sky!";
  messages[3] = "Goodbye, Volcano!";
  int sizes[4] = {33, 35, 31, 37};
  net::CompletionCallback write_callback;

  for (int i = 0; i < 4; i++) {
    EXPECT_CALL(mock_tcp_socket(),
                Write(A<net::IOBuffer*>(),
                      sizes[i],
                      A<const net::CompletionCallback&>()))
      .WillRepeatedly(DoAll(SaveArg<2>(&write_callback),
                            Return(net::ERR_IO_PENDING)));
    EXPECT_CALL(handler_, OnWriteComplete(sizes[i]));
  }

  for (int i = 0; i < 4; i++) {
    socket_->SendString("urn:test", messages[i],
                        base::Bind(&CompleteHandler::OnWriteComplete,
                                   base::Unretained(&handler_)));
  }
  for (int i = 0; i < 4; i++) {
    write_callback.Run(sizes[i]);
  }
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests error on writing.
TEST_F(CastSocketTest, TestWriteError) {
  ConnectHelper();
  net::CompletionCallback write_callback;

  EXPECT_CALL(mock_tcp_socket(),
              Write(A<net::IOBuffer*>(),
                    33,
                    A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<2>(&write_callback),
                    Return(net::ERR_SOCKET_NOT_CONNECTED)));
  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_SOCKET_NOT_CONNECTED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  socket_->SendString("urn:test", "Hello, World!",
                      base::Bind(&CompleteHandler::OnWriteComplete,
                                 base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Tests reading a single message.
TEST_F(CastSocketTest, TestRead) {
  net::CompletionCallback connect_callback;
  net::CompletionCallback read_callback;

  std::string message_data;
  ASSERT_TRUE(CastSocket::SerializeStringMessage("urn:test",
                                                 "Hello, World!",
                                                 &message_data));

  EXPECT_CALL(mock_tcp_socket(), SetNoDelay(true))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(mock_tcp_socket(), SetKeepAlive(true, A<int>()))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<0>(&connect_callback), Return(net::OK)));
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  EXPECT_CALL(mock_tcp_socket(), Read(A<net::IOBuffer*>(),
                                      A<int>(),
                                      A<const net::CompletionCallback&>()))
    .Times(3)
    .WillRepeatedly(DoAll(SaveArg<2>(&read_callback),
                          Return(net::ERR_IO_PENDING)));

  // Expect the test message to be read and invoke the delegate.
  EXPECT_CALL(mock_delegate_,
              OnStringMessage(socket_.get(), "urn:test", "Hello, World!"));

  // Connect the socket.
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback.Run(net::OK);

  // Put the test header and message into the io_buffers and invoke the read
  // callbacks.
  memcpy(socket_->header_read_buffer_->StartOfBuffer(),
         message_data.c_str(), 4);
  read_callback.Run(4);
  memcpy(socket_->body_read_buffer_->StartOfBuffer(),
         message_data.c_str() + 4, 29);
  read_callback.Run(29);

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests reading multiple messages.
TEST_F(CastSocketTest, TestReadMany) {
  net::CompletionCallback connect_callback;
  net::CompletionCallback read_callback;
  std::string messages[4];
  messages[0] = "Hello, World!";
  messages[1] = "Goodbye, World!";
  messages[2] = "Hello, Sky!";
  messages[3] = "Goodbye, Volcano!";
  int sizes[4] = {29, 31, 27, 33};
  std::string message_data[4];

  for (int i = 0; i < 4; i++)
    ASSERT_TRUE(CastSocket::SerializeStringMessage("urn:test",
                                                   messages[i],
                                                   &message_data[i]));

  EXPECT_CALL(mock_tcp_socket(), SetNoDelay(true))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(mock_tcp_socket(), SetKeepAlive(true, A<int>()))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<0>(&connect_callback), Return(net::OK)));
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  EXPECT_CALL(mock_tcp_socket(), Read(A<net::IOBuffer*>(),
                                      A<int>(),
                                      A<const net::CompletionCallback&>()))
    .Times(9)
    .WillRepeatedly(DoAll(SaveArg<2>(&read_callback),
                          Return(net::ERR_IO_PENDING)));

  // Expect the test messages to be read and invoke the delegate.
  for (int i = 0; i < 4; i++)
    EXPECT_CALL(mock_delegate_,
                OnStringMessage(socket_.get(), "urn:test", messages[i]));

  // Connect the socket.
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback.Run(net::OK);

  // Put the test headers and messages into the io_buffer and invoke the read
  // callbacks.
  for (int i = 0; i < 4; i++) {
    memcpy(socket_->header_read_buffer_->StartOfBuffer(),
           message_data[i].c_str(), 4);
    read_callback.Run(4);
    memcpy(socket_->body_read_buffer_->StartOfBuffer(),
           message_data[i].c_str() + 4, sizes[i]);
    read_callback.Run(sizes[i]);
  }

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests error on reading.
TEST_F(CastSocketTest, TestReadError) {
  net::CompletionCallback connect_callback;
  net::CompletionCallback read_callback;

  EXPECT_CALL(mock_tcp_socket(), SetNoDelay(true))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(mock_tcp_socket(), SetKeepAlive(true, A<int>()))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<0>(&connect_callback), Return(net::OK)));
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  EXPECT_CALL(mock_tcp_socket(), Read(A<net::IOBuffer*>(),
                                      A<int>(),
                                      A<const net::CompletionCallback&>()))
    .WillOnce(DoAll(SaveArg<2>(&read_callback),
                    Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));

  // Connect the socket.
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback.Run(net::OK);

  // Cause an error.
  read_callback.Run(net::ERR_SOCKET_NOT_CONNECTED);

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
