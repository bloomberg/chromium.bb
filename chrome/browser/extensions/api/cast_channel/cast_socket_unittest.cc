// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"

#include "chrome/browser/extensions/api/cast_channel/cast_channel.pb.h"
#include "chrome/browser/extensions/api/cast_channel/cast_message_util.h"
#include "net/base/address_list.h"
#include "net/base/capturing_net_log.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_info.h"
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
  MOCK_METHOD2(OnMessage, void(const CastSocket* socket,
                               const MessageInfo& message));
};

class MockTCPClientSocket : public net::TCPClientSocket {
 public:
  explicit MockTCPClientSocket() :
      TCPClientSocket(net::AddressList(), NULL, net::NetLog::Source()) { }
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

class MockSSLClientSocket : public net::MockClientSocket {
 public:
  MockSSLClientSocket() : MockClientSocket(net::BoundNetLog()) { }
  virtual ~MockSSLClientSocket() { }

  MOCK_METHOD1(Connect, int(const net::CompletionCallback& callback));
  MOCK_METHOD3(Read, int(net::IOBuffer* buf, int buf_len,
                         const net::CompletionCallback& callback));
  MOCK_METHOD3(Write, int(net::IOBuffer* buf, int buf_len,
                          const net::CompletionCallback& callback));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(WasEverUsed, bool());
  MOCK_CONST_METHOD0(UsingTCPFastOpen, bool());
  MOCK_CONST_METHOD0(WasNpnNegotiated, bool());
  MOCK_METHOD1(GetSSLInfo, bool(net::SSLInfo*));
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
  static scoped_ptr<TestCastSocket> Create(
      MockCastSocketDelegate* delegate) {
    return scoped_ptr<TestCastSocket>(
        new TestCastSocket(delegate, "cast://192.0.0.1:8009"));
  }

  static scoped_ptr<TestCastSocket> CreateSecure(
      MockCastSocketDelegate* delegate) {
    return scoped_ptr<TestCastSocket>(
        new TestCastSocket(delegate, "casts://192.0.0.1:8009"));
  }

  explicit TestCastSocket(MockCastSocketDelegate* delegate,
                          const std::string& url) :
    CastSocket("abcdefg", GURL(url), delegate,
               &capturing_net_log_),
    mock_tcp_socket_(new MockTCPClientSocket()),
    mock_ssl_socket_(new MockSSLClientSocket()),
    owns_tcp_socket_(true),
    owns_ssl_socket_(true),
    extract_cert_result_(true),
    send_auth_challenge_result_(net::ERR_IO_PENDING),
    read_auth_challenge_reply_result_(net::ERR_IO_PENDING),
    challenge_reply_result_(true) {
  }

  virtual ~TestCastSocket() {
    if (owns_tcp_socket_) {
      DCHECK(mock_tcp_socket_);
      delete mock_tcp_socket_;
    }
    if (owns_ssl_socket_) {
      DCHECK(mock_ssl_socket_);
      delete mock_ssl_socket_;
    }
  }

  virtual void Close(const net::CompletionCallback& callback) OVERRIDE {
    if (!owns_tcp_socket_)
      mock_tcp_socket_ = NULL;
    if (!owns_ssl_socket_)
      mock_ssl_socket_ = NULL;
    CastSocket::Close(callback);
  }

  void CreateNewSockets() {
    owns_tcp_socket_ = true;
    mock_tcp_socket_ = new MockTCPClientSocket();
    owns_ssl_socket_ = true;
    mock_ssl_socket_ = new MockSSLClientSocket();
  }

  void SetExtractCertResult(bool value) {
    extract_cert_result_ = value;
  }

  void SetSendAuthChallengeResult(int result) {
    send_auth_challenge_result_ = result;
  }

  void SetReadAuthChallengeReplyResult(int result) {
    read_auth_challenge_reply_result_ = result;
  }

  void SetChallengeReplyResult(bool value) {
    challenge_reply_result_ = value;
  }

  MockTCPClientSocket* mock_tcp_socket_;
  MockSSLClientSocket* mock_ssl_socket_;

 protected:
  virtual scoped_ptr<net::TCPClientSocket> CreateTcpSocket() OVERRIDE {
    owns_tcp_socket_ = false;
    return scoped_ptr<net::TCPClientSocket>(mock_tcp_socket_);
  }

  virtual scoped_ptr<net::SSLClientSocket> CreateSslSocket() OVERRIDE {
    owns_ssl_socket_ = false;
    return scoped_ptr<net::SSLClientSocket>(mock_ssl_socket_);
  }

  virtual bool ExtractPeerCert(std::string* cert) OVERRIDE {
    if (extract_cert_result_)
      cert->assign("dummy_test_cert");
    return extract_cert_result_;
  }

  virtual int SendAuthChallenge() OVERRIDE {
    return send_auth_challenge_result_;
  }

  virtual int ReadAuthChallengeReply() OVERRIDE {
    return read_auth_challenge_reply_result_;
  }

  virtual bool VerifyChallengeReply() OVERRIDE {
    return challenge_reply_result_;
  }

 private:
  net::CapturingNetLog capturing_net_log_;
  // Whether this object or the parent owns |mock_tcp_socket_|.
  bool owns_tcp_socket_;
  // Whether this object or the parent owns |mock_ssl_socket_|.
  bool owns_ssl_socket_;
  // Simulated result of peer cert extraction.
  bool extract_cert_result_;
  // Simulated result to be returned by SendAuthChallenge.
  int send_auth_challenge_result_;
  // Simulated result to be returned by ReadAuthChallengeReply.
  int read_auth_challenge_reply_result_;
  // Simulated result of verifying challenge reply.
  bool challenge_reply_result_;
};

class CastSocketTest : public testing::Test {
 public:
  CastSocketTest() {}
  virtual ~CastSocketTest() {}

  virtual void SetUp() OVERRIDE {
    test_message_.namespace_ = "urn:test";
    test_message_.source_id = "1";
    test_message_.destination_id = "2";
    test_message_.data.reset(new base::StringValue("Hello, World!"));
    ASSERT_TRUE(MessageInfoToCastMessage(test_message_, &test_proto_));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_CALL(handler_, OnCloseComplete(net::OK));
    socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                              base::Unretained(&handler_)));
  }

  void CreateCastSocket() {
    socket_ = TestCastSocket::Create(&mock_delegate_);
  }

  void CreateCastSocketSecure() {
    socket_ = TestCastSocket::CreateSecure(&mock_delegate_);
  }

  // Sets an expectation that TCPClientSocket::Connect is called and
  // returns |result| and stores the callback passed to it in |callback|.
  void ExpectTcpConnect(net::CompletionCallback* callback, int result) {
    EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(callback), Return(result)));
  }

  // Same as ExpectTcpConnect but to return net::ERR_IO_PENDING.
  void ExpectTcpConnectPending(net::CompletionCallback* callback) {
    ExpectTcpConnect(callback, net::ERR_IO_PENDING);
  }

  // Sets an expectation that SSLClientSocket::Connect is called and
  // returns |result| and stores the callback passed to it in |callback|.
  void ExpectSslConnect(net::CompletionCallback* callback, int result) {
    EXPECT_CALL(mock_ssl_socket(), Connect(A<const net::CompletionCallback&>()))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(callback), Return(result)));
  }

  // Same as ExpectSslConnect but to return net::ERR_IO_PENDING.
  void ExpectSslConnectPending(net::CompletionCallback* callback) {
    ExpectSslConnect(callback, net::ERR_IO_PENDING);
  }

  // Sets an expectation that SSLClientSocket::Read is called |times| number
  // of times and returns net::ERR_IO_PENDING.
  void ExpectSslRead(int times) {
    EXPECT_CALL(mock_ssl_socket(), Read(A<net::IOBuffer*>(),
                                        A<int>(),
                                        A<const net::CompletionCallback&>()))
        .Times(times)
        .WillOnce(Return(net::ERR_IO_PENDING));
  }

  // Sets up CastSocket::Connect to succeed.
  // Connecting the socket also starts the read loop; we expect the call to
  // Read(), but never fire the read callback.
  void ConnectHelper() {
    net::CompletionCallback connect_callback1;
    net::CompletionCallback connect_callback2;

    ExpectTcpConnect(&connect_callback1, net::OK);
    ExpectSslConnect(&connect_callback2, net::OK);
    EXPECT_CALL(handler_, OnConnectComplete(net::OK));
    ExpectSslRead(1);

    socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                                base::Unretained(&handler_)));

    EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
    EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
  }

 protected:
  MockTCPClientSocket& mock_tcp_socket() {
    MockTCPClientSocket* mock_socket = socket_->mock_tcp_socket_;
    DCHECK(mock_socket);
    return *mock_socket;
  }

  MockSSLClientSocket& mock_ssl_socket() {
    MockSSLClientSocket* mock_socket = socket_->mock_ssl_socket_;
    DCHECK(mock_socket);
    return *mock_socket;
  }

  void CallOnChallengeEvent(int result) {
    socket_->OnChallengeEvent(result);
  }

  MockCastSocketDelegate mock_delegate_;
  scoped_ptr<TestCastSocket> socket_;
  CompleteHandler handler_;
  MessageInfo test_message_;
  CastMessage test_proto_;
};

// Tests URL parsing and validation.
TEST_F(CastSocketTest, TestCastURLs) {
  CreateCastSocket();
  EXPECT_TRUE(socket_->ParseChannelUrl(GURL("cast://192.0.0.1:8009")));
  EXPECT_FALSE(socket_->auth_required());
  EXPECT_EQ(socket_->ip_endpoint_.ToString(), "192.0.0.1:8009");

  EXPECT_TRUE(socket_->ParseChannelUrl(GURL("casts://192.0.0.1:12345")));
  EXPECT_TRUE(socket_->auth_required());
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
  CreateCastSocket();
  ConnectHelper();

  EXPECT_CALL(handler_, OnCloseComplete(net::OK));
  socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                            base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection succeeds (async)
TEST_F(CastSocketTest, TestConnect) {
  CreateCastSocket();

  net::CompletionCallback connect_callback1;
  net::CompletionCallback connect_callback2;

  ExpectTcpConnectPending(&connect_callback1);
  ExpectSslConnectPending(&connect_callback2);
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  ExpectSslRead(1);

  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback1.Run(net::OK);
  connect_callback2.Run(net::OK);

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection succeeds (async)
TEST_F(CastSocketTest, TestTwoStepConnect) {
  CreateCastSocket();

  // Expectations for the initial connect call
  net::CompletionCallback tcp_connect_callback1;
  net::CompletionCallback ssl_connect_callback1;

  ExpectTcpConnectPending(&tcp_connect_callback1);
  ExpectSslConnectPending(&ssl_connect_callback1);

  // Start connect flow
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  tcp_connect_callback1.Run(net::OK);

  // Expectations for the second connect call
  socket_->CreateNewSockets();
  net::CompletionCallback tcp_connect_callback2;
  net::CompletionCallback ssl_connect_callback2;
  ExpectTcpConnectPending(&tcp_connect_callback2);
  ExpectSslConnectPending(&ssl_connect_callback2);
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  ExpectSslRead(1);

  // Trigger callbacks for the first connect
  ssl_connect_callback1.Run(net::ERR_CERT_AUTHORITY_INVALID);

  // Trigger callbacks for the second connect
  tcp_connect_callback2.Run(net::OK);
  ssl_connect_callback2.Run(net::OK);

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection fails (async)
// - The flow should NOT be tried again
TEST_F(CastSocketTest, TestMaxTwoConnectAttempts) {
  CreateCastSocket();

  net::CompletionCallback tcp_connect_callback1;
  net::CompletionCallback ssl_connect_callback1;

  // Expectations for the initial connect call
  ExpectTcpConnectPending(&tcp_connect_callback1);
  ExpectSslConnectPending(&ssl_connect_callback1);

  // Start connect flow
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  tcp_connect_callback1.Run(net::OK);

  socket_->CreateNewSockets();
  net::CompletionCallback tcp_connect_callback2;
  net::CompletionCallback ssl_connect_callback2;

  // Expectations for the second connect call
  ExpectTcpConnectPending(&tcp_connect_callback2);
  ExpectSslConnectPending(&ssl_connect_callback2);
  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CERT_AUTHORITY_INVALID));

  // Trigger callbacks for the first connect
  ssl_connect_callback1.Run(net::ERR_CERT_AUTHORITY_INVALID);

  // Trigger callbacks for the second connect
  tcp_connect_callback2.Run(net::OK);
  ssl_connect_callback2.Run(net::ERR_CERT_AUTHORITY_INVALID);

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test that when cert extraction fails the connection flow stops.
TEST_F(CastSocketTest, TestCertExtractionFailure) {
  CreateCastSocket();

  net::CompletionCallback connect_callback1;
  net::CompletionCallback connect_callback2;

  ExpectTcpConnectPending(&connect_callback1);
  ExpectSslConnectPending(&connect_callback2);

  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback1.Run(net::OK);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CERT_AUTHORITY_INVALID));

  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);
  // Attempt to connect results in ERR_CERT_AUTHORTY_INVALID
  connect_callback2.Run(net::ERR_CERT_AUTHORITY_INVALID);

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Tests that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection succeeds (async)
// - Challenge request is sent (async)
// - Challenge response is received (async)
// - Credentials are verified successfuly
TEST_F(CastSocketTest, TestFullSecureConnectionFlowAsync) {
  CreateCastSocketSecure();

  net::CompletionCallback tcp_connect_callback1;
  net::CompletionCallback ssl_connect_callback1;

  // Expectations for the initial connect call
  ExpectTcpConnectPending(&tcp_connect_callback1);
  ExpectSslConnectPending(&ssl_connect_callback1);

  // Start connect flow
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  tcp_connect_callback1.Run(net::OK);

  socket_->CreateNewSockets();
  net::CompletionCallback tcp_connect_callback2;
  net::CompletionCallback ssl_connect_callback2;

  // Expectations for the second connect call
  ExpectTcpConnectPending(&tcp_connect_callback2);
  ExpectSslConnectPending(&ssl_connect_callback2);
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  ExpectSslRead(1);

  // Trigger callbacks for the first connect
  ssl_connect_callback1.Run(net::ERR_CERT_AUTHORITY_INVALID);

  // Trigger callbacks for the second connect
  tcp_connect_callback2.Run(net::OK);
  ssl_connect_callback2.Run(net::OK);

  // Trigger callbacks for auth events.
  CallOnChallengeEvent(net::OK);  // Sent challenge
  CallOnChallengeEvent(net::OK);  // Received reply

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Same as TestFullSecureConnectionFlowAsync, but operations are  synchronous.
TEST_F(CastSocketTest, TestFullSecureConnectionFlowSync) {
  CreateCastSocketSecure();

  net::CompletionCallback tcp_connect_callback;
  net::CompletionCallback ssl_connect_callback;

  // Expectations for the connect calls
  ExpectTcpConnect(&tcp_connect_callback, net::OK);
  ExpectSslConnect(&ssl_connect_callback, net::OK);
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  ExpectSslRead(1);

  socket_->SetSendAuthChallengeResult(net::OK);
  socket_->SetReadAuthChallengeReplyResult(net::OK);

  // Start connect flow
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests writing a single message where the completion is signaled via
// callback.
TEST_F(CastSocketTest, TestWriteViaCallback) {
  CreateCastSocket();
  ConnectHelper();

  net::CompletionCallback write_callback;

  EXPECT_CALL(mock_ssl_socket(),
              Write(A<net::IOBuffer*>(),
                    39,
                    A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<2>(&write_callback),
                    Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(handler_, OnWriteComplete(39));
  socket_->SendMessage(test_message_,
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  write_callback.Run(39);
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests writing a single message where the Write() returns directly.
TEST_F(CastSocketTest, TestWrite) {
  CreateCastSocket();
  ConnectHelper();

  EXPECT_CALL(mock_ssl_socket(),
              Write(A<net::IOBuffer*>(),
                    39,
                    A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(Return(39));
  EXPECT_CALL(handler_, OnWriteComplete(39));
  socket_->SendMessage(test_message_,
                      base::Bind(&CompleteHandler::OnWriteComplete,
                                 base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests writing multiple messages.
TEST_F(CastSocketTest, TestWriteMany) {
  CreateCastSocket();
  ConnectHelper();
  std::string messages[4];
  messages[0] = "Hello, World!";
  messages[1] = "Goodbye, World!";
  messages[2] = "Hello, Sky!";
  messages[3] = "Goodbye, Volcano!";
  int sizes[4] = {39, 41, 37, 43};
  MessageInfo message_info[4];
  net::CompletionCallback write_callback;

  for (int i = 0; i < 4; i++) {
    EXPECT_CALL(mock_ssl_socket(),
                Write(A<net::IOBuffer*>(),
                      sizes[i],
                      A<const net::CompletionCallback&>()))
      .WillRepeatedly(DoAll(SaveArg<2>(&write_callback),
                            Return(net::ERR_IO_PENDING)));
    EXPECT_CALL(handler_, OnWriteComplete(sizes[i]));
  }

  for (int i = 0; i < 4; i++) {
    message_info[i].namespace_ = "urn:test";
    message_info[i].source_id = "1";
    message_info[i].destination_id = "2";
    message_info[i].data.reset(new base::StringValue(messages[i]));
    socket_->SendMessage(message_info[i],
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
  CreateCastSocket();
  ConnectHelper();
  net::CompletionCallback write_callback;

  EXPECT_CALL(mock_ssl_socket(),
              Write(A<net::IOBuffer*>(),
                    39,
                    A<const net::CompletionCallback&>()))
    .Times(1)
    .WillOnce(DoAll(SaveArg<2>(&write_callback),
                    Return(net::ERR_SOCKET_NOT_CONNECTED)));
  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_SOCKET_NOT_CONNECTED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));
  socket_->SendMessage(test_message_,
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Tests reading a single message.
TEST_F(CastSocketTest, TestRead) {
  CreateCastSocket();

  net::CompletionCallback connect_callback1;
  net::CompletionCallback connect_callback2;
  net::CompletionCallback read_callback;

  std::string message_data;
  ASSERT_TRUE(CastSocket::Serialize(test_proto_, &message_data));

  EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback1),
                      Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(mock_ssl_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback2),
                        Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  EXPECT_CALL(mock_ssl_socket(), Read(A<net::IOBuffer*>(),
                                      A<int>(),
                                      A<const net::CompletionCallback&>()))
      .Times(3)
      .WillRepeatedly(DoAll(SaveArg<2>(&read_callback),
                            Return(net::ERR_IO_PENDING)));

  // Expect the test message to be read and invoke the delegate.
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));

  // Connect the socket.
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback1.Run(net::OK);
  connect_callback2.Run(net::OK);

  // Put the test header and message into the io_buffers and invoke the read
  // callbacks.
  memcpy(socket_->header_read_buffer_->StartOfBuffer(),
         message_data.c_str(), 4);
  read_callback.Run(4);
  memcpy(socket_->body_read_buffer_->StartOfBuffer(),
         message_data.c_str() + 4, 35);
  read_callback.Run(35);

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Tests reading multiple messages.
TEST_F(CastSocketTest, TestReadMany) {
  CreateCastSocket();

  net::CompletionCallback connect_callback1;
  net::CompletionCallback connect_callback2;
  net::CompletionCallback read_callback;

  std::string messages[4];
  messages[0] = "Hello, World!";
  messages[1] = "Goodbye, World!";
  messages[2] = "Hello, Sky!";
  messages[3] = "Goodbye, Volcano!";
  int sizes[4] = {35, 37, 33, 39};
  std::string message_data[4];

  // Set up test data
  for (int i = 0; i < 4; i++) {
    test_proto_.set_payload_utf8(messages[i]);
    ASSERT_TRUE(CastSocket::Serialize(test_proto_, &message_data[i]));
  }

  EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback1),
                      Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(mock_ssl_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback2),
                        Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  EXPECT_CALL(mock_ssl_socket(), Read(A<net::IOBuffer*>(),
                                      A<int>(),
                                      A<const net::CompletionCallback&>()))
    .Times(9)
    .WillRepeatedly(DoAll(SaveArg<2>(&read_callback),
                          Return(net::ERR_IO_PENDING)));

  // Expect the test messages to be read and invoke the delegate.
  EXPECT_CALL(mock_delegate_, OnMessage(socket_.get(),
                                        A<const MessageInfo&>()))
    .Times(4);

  // Connect the socket.
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback1.Run(net::OK);
  connect_callback2.Run(net::OK);

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
  CreateCastSocket();

  net::CompletionCallback connect_callback1;
  net::CompletionCallback connect_callback2;
  net::CompletionCallback read_callback;

  EXPECT_CALL(mock_tcp_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback1),
                      Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(mock_ssl_socket(), Connect(A<const net::CompletionCallback&>()))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&connect_callback2),
                        Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  EXPECT_CALL(mock_ssl_socket(), Read(A<net::IOBuffer*>(),
                                      A<int>(),
                                      A<const net::CompletionCallback&>()))
    .WillOnce(DoAll(SaveArg<2>(&read_callback),
                    Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(), cast_channel::CHANNEL_ERROR_SOCKET_ERROR));

  // Connect the socket.
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  connect_callback1.Run(net::OK);
  connect_callback2.Run(net::OK);

  // Cause an error.
  read_callback.Run(net::ERR_SOCKET_NOT_CONNECTED);

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
