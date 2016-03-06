// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/socket/mock_tcp_client_socket.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

const int kBufferLength = 10;

template <typename T>
scoped_ptr<T> CreateTestSocket(scoped_ptr<MockTCPClientSocket> stream);

template <>
scoped_ptr<TCPSocket> CreateTestSocket(scoped_ptr<MockTCPClientSocket> stream) {
  return make_scoped_ptr(new TCPSocket(std::move(stream), "fake id",
                         true /* is_connected */));
}

template <>
scoped_ptr<TLSSocket> CreateTestSocket(scoped_ptr<MockTCPClientSocket> stream) {
  return make_scoped_ptr(new TLSSocket(std::move(stream), "fake id"));
}

class CombinedSocketTest : public testing::Test {
 public:
  CombinedSocketTest() : count_(0), io_buffer_(nullptr) {}

  // Strict test for synchronous (immediate) read behavior
  template <typename T>
  void TestRead() {
    net::IOBuffer* buffer = nullptr;

    scoped_ptr<MockTCPClientSocket> stream(
        new testing::StrictMock<MockTCPClientSocket>());
    EXPECT_CALL(*stream, Read(testing::NotNull(), kBufferLength, testing::_))
        .WillOnce(DoAll(testing::SaveArg<0>(&buffer),
                        testing::Return(kBufferLength)));
    EXPECT_CALL(*stream, Disconnect());

    scoped_ptr<T> socket = CreateTestSocket<T>(std::move(stream));
    ReadCompletionCallback read_callback =
        base::Bind(&CombinedSocketTest::OnRead, base::Unretained(this));
    socket->Read(kBufferLength, read_callback);
    EXPECT_EQ(kBufferLength, count_);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(buffer, io_buffer_);
  }

  // Strict test for async read behavior (read returns PENDING)
  template <typename T>
  void TestReadPending() {
    net::IOBuffer* buffer = nullptr;
    net::CompletionCallback socket_cb;

    scoped_ptr<MockTCPClientSocket> stream(
        new testing::StrictMock<MockTCPClientSocket>());
    EXPECT_CALL(*stream, Read(testing::NotNull(), kBufferLength, testing::_))
        .WillOnce(DoAll(testing::SaveArg<0>(&buffer),
                        testing::SaveArg<2>(&socket_cb),
                        testing::Return(net::ERR_IO_PENDING)));
    EXPECT_CALL(*stream, Disconnect());

    scoped_ptr<T> socket = CreateTestSocket<T>(std::move(stream));
    ReadCompletionCallback read_callback =
        base::Bind(&CombinedSocketTest::OnRead, base::Unretained(this));
    socket->Read(kBufferLength, read_callback);
    EXPECT_EQ(0, count_);
    EXPECT_EQ(nullptr, io_buffer_);
    socket_cb.Run(kBufferLength);
    EXPECT_EQ(kBufferLength, count_);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(buffer, io_buffer_);
  }

  // Even if the socket is closed, it may still have data left to read.
  template <typename T>
  void TestReadAfterDisconnect() {
    net::IOBuffer* buffer = nullptr;

    scoped_ptr<MockTCPClientSocket> stream(
        new testing::NiceMock<MockTCPClientSocket>());
    EXPECT_CALL(*stream, Read(testing::NotNull(), kBufferLength, testing::_))
        .WillOnce(DoAll(testing::SaveArg<0>(&buffer),
                        testing::Return(kBufferLength)));
    ON_CALL(*stream, IsConnected()).WillByDefault(testing::Return(false));
    EXPECT_CALL(*stream, Disconnect());

    scoped_ptr<T> socket = CreateTestSocket<T>(std::move(stream));
    ReadCompletionCallback read_callback =
        base::Bind(&CombinedSocketTest::OnRead, base::Unretained(this));
    socket->Read(kBufferLength, read_callback);
    EXPECT_EQ(kBufferLength, count_);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(buffer, io_buffer_);
  }

  void OnRead(int count, scoped_refptr<net::IOBuffer> io_buffer) {
    count_ = count;
    io_buffer_ = io_buffer.get();
  }

 protected:
  int count_;
  net::IOBuffer* io_buffer_;
};

TEST_F(CombinedSocketTest, TlsRead) {
  TestRead<TLSSocket>();
}

TEST_F(CombinedSocketTest, TcpRead) {
  TestRead<TCPSocket>();
}

TEST_F(CombinedSocketTest, TlsReadPending) {
  TestReadPending<TLSSocket>();
}

TEST_F(CombinedSocketTest, TcpReadPending) {
  TestReadPending<TCPSocket>();
}

TEST_F(CombinedSocketTest, TlsReadAfterDisconnect) {
  TestReadAfterDisconnect<TLSSocket>();
}

TEST_F(CombinedSocketTest, TcpReadAfterDisconnect) {
  TestReadAfterDisconnect<TCPSocket>();
}

}  //  namespace extensions
