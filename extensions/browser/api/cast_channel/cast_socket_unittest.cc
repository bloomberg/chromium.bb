// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_socket.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "extensions/browser/api/cast_channel/cast_framer.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/base/address_list.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

const int64 kDistantTimeoutMillis = 100000;  // 100 seconds (never hit).

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

namespace {
const char* kTestData[4] = {
    "Hello, World!",
    "Goodbye, World!",
    "Hello, Sky!",
    "Goodbye, Volcano!",
};
}  // namespace

namespace extensions {
namespace core_api {
namespace cast_channel {

// Fills in |message| with a string message.
static void CreateStringMessage(const std::string& namespace_,
                                const std::string& source_id,
                                const std::string& destination_id,
                                const std::string& data,
                                MessageInfo* message) {
  message->namespace_ = namespace_;
  message->source_id = source_id;
  message->destination_id = destination_id;
  message->data.reset(new base::StringValue(data));
}

// Fills in |message| with a binary message.
static void CreateBinaryMessage(const std::string& namespace_,
                                const std::string& source_id,
                                const std::string& destination_id,
                                const std::string& data,
                                MessageInfo* message) {
  message->namespace_ = namespace_;
  message->source_id = source_id;
  message->destination_id = destination_id;
  message->data.reset(base::BinaryValue::CreateWithCopiedBuffer(
      data.c_str(), data.size()));
}

class MockCastSocketDelegate : public CastSocket::Delegate {
 public:
  MOCK_METHOD3(OnError,
               void(const CastSocket* socket,
                    ChannelError error,
                    const LastErrors& last_errors));
  MOCK_METHOD2(OnMessage,
               void(const CastSocket* socket, const MessageInfo& message));
};

class MockTCPSocket : public net::TCPClientSocket {
 public:
  explicit MockTCPSocket(const net::MockConnect& connect_data) :
      TCPClientSocket(net::AddressList(), NULL, net::NetLog::Source()),
      connect_data_(connect_data),
      do_nothing_(false) { }

  explicit MockTCPSocket(bool do_nothing) :
      TCPClientSocket(net::AddressList(), NULL, net::NetLog::Source()) {
    CHECK(do_nothing);
    do_nothing_ = do_nothing;
  }

  virtual int Connect(const net::CompletionCallback& callback) OVERRIDE {
    if (do_nothing_) {
      // Stall the I/O event loop.
      return net::ERR_IO_PENDING;
    }

    if (connect_data_.mode == net::ASYNC) {
      CHECK_NE(connect_data_.result, net::ERR_IO_PENDING);
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, connect_data_.result));
      return net::ERR_IO_PENDING;
    } else {
      return connect_data_.result;
    }
  }

  virtual bool SetKeepAlive(bool enable, int delay) OVERRIDE {
    // Always return true in tests
    return true;
  }

  virtual bool SetNoDelay(bool no_delay) OVERRIDE {
    // Always return true in tests
    return true;
  }

  MOCK_METHOD3(Read,
               int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write,
               int(net::IOBuffer*, int, const net::CompletionCallback&));

  virtual void Disconnect() OVERRIDE {
    // Do nothing in tests
  }

 private:
  net::MockConnect connect_data_;
  bool do_nothing_;
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
  static scoped_ptr<TestCastSocket> Create(MockCastSocketDelegate* delegate,
                                           Logger* logger) {
    return scoped_ptr<TestCastSocket>(new TestCastSocket(delegate,
                                                         CreateIPEndPoint(),
                                                         CHANNEL_AUTH_TYPE_SSL,
                                                         kDistantTimeoutMillis,
                                                         logger));
  }

  static scoped_ptr<TestCastSocket> CreateSecure(
      MockCastSocketDelegate* delegate,
      Logger* logger) {
    return scoped_ptr<TestCastSocket>(
        new TestCastSocket(delegate,
                           CreateIPEndPoint(),
                           CHANNEL_AUTH_TYPE_SSL_VERIFIED,
                           kDistantTimeoutMillis,
                           logger));
  }

  explicit TestCastSocket(MockCastSocketDelegate* delegate,
                          const net::IPEndPoint& ip_endpoint,
                          ChannelAuthType channel_auth,
                          int64 timeout_ms,
                          Logger* logger)
      : CastSocket("abcdefg",
                   ip_endpoint,
                   channel_auth,
                   delegate,
                   &capturing_net_log_,
                   base::TimeDelta::FromMilliseconds(timeout_ms),
                   logger),
        ip_(ip_endpoint),
        connect_index_(0),
        extract_cert_result_(true),
        verify_challenge_result_(true),
        verify_challenge_disallow_(false),
        tcp_unresponsive_(false),
        mock_timer_(new base::MockTimer(false, false)) {}

  static net::IPEndPoint CreateIPEndPoint() {
    net::IPAddressNumber number;
    number.push_back(192);
    number.push_back(0);
    number.push_back(0);
    number.push_back(1);
    return net::IPEndPoint(number, 8009);
  }

  // Returns the size of the body (in bytes) of the given serialized message.
  static size_t ComputeBodySize(const std::string& msg) {
    return msg.length() - MessageFramer::MessageHeader::header_size();
  }

  virtual ~TestCastSocket() {}

  // Helpers to set mock results for various operations.
  void SetupTcp1Connect(net::IoMode mode, int result) {
    tcp_connect_data_[0].reset(new net::MockConnect(mode, result));
  }
  void SetupSsl1Connect(net::IoMode mode, int result) {
    ssl_connect_data_[0].reset(new net::MockConnect(mode, result));
  }
  void SetupTcp2Connect(net::IoMode mode, int result) {
    tcp_connect_data_[1].reset(new net::MockConnect(mode, result));
  }
  void SetupSsl2Connect(net::IoMode mode, int result) {
    ssl_connect_data_[1].reset(new net::MockConnect(mode, result));
  }
  void SetupTcp1ConnectUnresponsive() {
    tcp_unresponsive_ = true;
  }
  void AddWriteResult(const net::MockWrite& write) {
    writes_.push_back(write);
  }
  void AddWriteResult(net::IoMode mode, int result) {
    AddWriteResult(net::MockWrite(mode, result));
  }
  void AddWriteResultForMessage(net::IoMode mode, const std::string& msg) {
    AddWriteResult(mode, msg.size());
  }
  void AddWriteResultForMessage(net::IoMode mode,
                                const std::string& msg,
                                size_t ch_size) {
    size_t msg_size = msg.size();
    for (size_t offset = 0; offset < msg_size; offset += ch_size) {
      if (offset + ch_size > msg_size)
        ch_size = msg_size - offset;
      AddWriteResult(mode, ch_size);
    }
  }

  void AddReadResult(const net::MockRead& read) {
    reads_.push_back(read);
  }
  void AddReadResult(net::IoMode mode, int result) {
    AddReadResult(net::MockRead(mode, result));
  }
  void AddReadResult(net::IoMode mode, const char* data, int data_len) {
    AddReadResult(net::MockRead(mode, data, data_len));
  }
  void AddReadResultForMessage(net::IoMode mode, const std::string& msg) {
    size_t body_size = ComputeBodySize(msg);
    const char* data = msg.c_str();
    AddReadResult(mode, data, MessageFramer::MessageHeader::header_size());
    AddReadResult(
        mode, data + MessageFramer::MessageHeader::header_size(), body_size);
  }
  void AddReadResultForMessage(net::IoMode mode,
                               const std::string& msg,
                               size_t ch_size) {
    size_t msg_size = msg.size();
    const char* data = msg.c_str();
    for (size_t offset = 0; offset < msg_size; offset += ch_size) {
      if (offset + ch_size > msg_size)
        ch_size = msg_size - offset;
      AddReadResult(mode, data + offset, ch_size);
    }
  }

  void SetExtractCertResult(bool value) {
    extract_cert_result_ = value;
  }
  void SetVerifyChallengeResult(bool value) {
    verify_challenge_result_ = value;
  }

  void TriggerTimeout() {
    mock_timer_->Fire();
  }

  void DisallowVerifyChallengeResult() { verify_challenge_disallow_ = true; }

 private:
  virtual scoped_ptr<net::TCPClientSocket> CreateTcpSocket() OVERRIDE {
    if (tcp_unresponsive_) {
      return scoped_ptr<net::TCPClientSocket>(new MockTCPSocket(true));
    } else {
      net::MockConnect* connect_data = tcp_connect_data_[connect_index_].get();
      connect_data->peer_addr = ip_;
      return scoped_ptr<net::TCPClientSocket>(new MockTCPSocket(*connect_data));
    }
  }

  virtual scoped_ptr<net::SSLClientSocket> CreateSslSocket(
      scoped_ptr<net::StreamSocket> socket) OVERRIDE {
    net::MockConnect* connect_data = ssl_connect_data_[connect_index_].get();
    connect_data->peer_addr = ip_;
    ++connect_index_;

    ssl_data_.reset(new net::StaticSocketDataProvider(
        reads_.data(), reads_.size(), writes_.data(), writes_.size()));
    ssl_data_->set_connect_data(*connect_data);
    // NOTE: net::MockTCPClientSocket inherits from net::SSLClientSocket !!
    return scoped_ptr<net::SSLClientSocket>(
        new net::MockTCPClientSocket(
            net::AddressList(), &capturing_net_log_, ssl_data_.get()));
  }

  virtual bool ExtractPeerCert(std::string* cert) OVERRIDE {
    if (extract_cert_result_)
      cert->assign("dummy_test_cert");
    return extract_cert_result_;
  }

  virtual bool VerifyChallengeReply() OVERRIDE {
    EXPECT_FALSE(verify_challenge_disallow_);
    return verify_challenge_result_;
  }

  virtual base::Timer* GetTimer() OVERRIDE {
    return mock_timer_.get();
  }

  net::CapturingNetLog capturing_net_log_;
  net::IPEndPoint ip_;
  // Simulated connect data
  scoped_ptr<net::MockConnect> tcp_connect_data_[2];
  scoped_ptr<net::MockConnect> ssl_connect_data_[2];
  // Simulated read / write data
  std::vector<net::MockWrite> writes_;
  std::vector<net::MockRead> reads_;
  scoped_ptr<net::SocketDataProvider> ssl_data_;
  // Number of times Connect method is called
  size_t connect_index_;
  // Simulated result of peer cert extraction.
  bool extract_cert_result_;
  // Simulated result of verifying challenge reply.
  bool verify_challenge_result_;
  bool verify_challenge_disallow_;
  // If true, makes TCP connection process stall. For timeout testing.
  bool tcp_unresponsive_;
  scoped_ptr<base::MockTimer> mock_timer_;
};

class CastSocketTest : public testing::Test {
 public:
  CastSocketTest()
      : logger_(new Logger(
            scoped_ptr<base::TickClock>(new base::SimpleTestTickClock),
            base::TimeTicks())) {}
  virtual ~CastSocketTest() {}

  virtual void SetUp() OVERRIDE {
    // Create a few test messages
    for (size_t i = 0; i < arraysize(test_messages_); i++) {
      CreateStringMessage("urn:cast", "1", "2", kTestData[i],
                          &test_messages_[i]);
      ASSERT_TRUE(MessageInfoToCastMessage(
          test_messages_[i], &test_protos_[i]));
      ASSERT_TRUE(
          MessageFramer::Serialize(test_protos_[i], &test_proto_strs_[i]));
    }
  }

  virtual void TearDown() OVERRIDE {
    if (socket_.get()) {
      EXPECT_CALL(handler_, OnCloseComplete(net::OK));
      socket_->Close(base::Bind(&CompleteHandler::OnCloseComplete,
                                base::Unretained(&handler_)));
    }
  }

  // The caller can specify non-standard namespaces by setting "auth_namespace"
  // (useful for negative test cases.)
  void SetupAuthMessage(
      const char* auth_namespace = "urn:x-cast:com.google.cast.tp.deviceauth") {
    // Create a test auth request.
    CastMessage request;
    CreateAuthChallengeMessage(&request);
    ASSERT_TRUE(MessageFramer::Serialize(request, &auth_request_));

    // Create a test auth reply.
    MessageInfo reply;
    CreateBinaryMessage(
        auth_namespace, "sender-0", "receiver-0", "abcd", &reply);
    CastMessage reply_msg;
    ASSERT_TRUE(MessageInfoToCastMessage(reply, &reply_msg));
    ASSERT_TRUE(MessageFramer::Serialize(reply_msg, &auth_reply_));
  }

  void CreateCastSocket() {
    socket_ = TestCastSocket::Create(&mock_delegate_, logger_.get());
  }

  void CreateCastSocketSecure() {
    socket_ = TestCastSocket::CreateSecure(&mock_delegate_, logger_.get());
  }

  // Sets up CastSocket::Connect to succeed.
  // Connecting the socket also starts the read loop; so we add a mock
  // read result that returns IO_PENDING and callback is never fired.
  void ConnectHelper() {
    socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
    socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
    socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

    EXPECT_CALL(handler_, OnConnectComplete(net::OK));
    socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                                base::Unretained(&handler_)));
    RunPendingTasks();
  }

 protected:
  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  MockCastSocketDelegate mock_delegate_;
  scoped_refptr<Logger> logger_;
  scoped_ptr<TestCastSocket> socket_;
  CompleteHandler handler_;
  MessageInfo test_messages_[arraysize(kTestData)];
  CastMessage test_protos_[arraysize(kTestData)];
  std::string test_proto_strs_[arraysize(kTestData)];
  std::string auth_request_;
  std::string auth_reply_;
};

// Tests connecting and closing the socket.
TEST_F(CastSocketTest, TestConnectAndClose) {
  CreateCastSocket();
  ConnectHelper();
  SetupAuthMessage();
  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());

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
  SetupAuthMessage();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::OK);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that the following connection flow works:
// - TCP connection succeeds (async)
// - SSL connection fails with cert error (async)
// - Cert is extracted successfully
// - Second TCP connection succeeds (async)
// - Second SSL connection succeeds (async)
TEST_F(CastSocketTest, TestConnectTwoStep) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::OK);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

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
TEST_F(CastSocketTest, TestConnectMaxTwoAttempts) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

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
TEST_F(CastSocketTest, TestConnectFullSecureFlowAsync) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResultForMessage(net::ASYNC, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Same as TestFullSecureConnectionFlowAsync, but operations are synchronous.
TEST_F(CastSocketTest, TestConnectFullSecureFlowSync) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl2Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResultForMessage(net::SYNCHRONOUS, auth_request_);
  socket_->AddReadResultForMessage(net::SYNCHRONOUS, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);

  EXPECT_CALL(handler_, OnConnectComplete(net::OK));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test that an AuthMessage with a mangled namespace triggers cancelation
// of the connection event loop.
TEST_F(CastSocketTest, TestConnectAuthMessageCorrupted) {
  CreateCastSocketSecure();
  SetupAuthMessage("bogus_namespace");

  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  socket_->SetupTcp2Connect(net::ASYNC, net::OK);
  socket_->SetupSsl2Connect(net::ASYNC, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResultForMessage(net::ASYNC, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);
  // Guard against VerifyChallengeResult() being triggered.
  socket_->DisallowVerifyChallengeResult();

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - TCP connect fails (async)
TEST_F(CastSocketTest, TestConnectTcpConnectErrorAsync) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - TCP connect fails (sync)
TEST_F(CastSocketTest, TestConnectTcpConnectErrorSync) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - timeout
TEST_F(CastSocketTest, TestConnectTcpTimeoutError) {
  CreateCastSocketSecure();
  socket_->SetupTcp1ConnectUnresponsive();
  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_TIMEOUT,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CONNECTING, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
  socket_->TriggerTimeout();
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_TIMEOUT,
            socket_->error_state());
}

// Test connection error - SSL connect fails (async)
TEST_F(CastSocketTest, TestConnectSslConnectErrorAsync) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - SSL connect fails (sync)
TEST_F(CastSocketTest, TestConnectSslConnectErrorSync) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - cert extraction error (async)
TEST_F(CastSocketTest, TestConnectCertExtractionErrorAsync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->SetupTcp1Connect(net::ASYNC, net::OK);
  socket_->SetupSsl1Connect(net::ASYNC, net::ERR_CERT_AUTHORITY_INVALID);
  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - cert extraction error (sync)
TEST_F(CastSocketTest, TestConnectCertExtractionErrorSync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::ERR_CERT_AUTHORITY_INVALID);
  // Set cert extraction to fail
  socket_->SetExtractCertResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - challenge send fails
TEST_F(CastSocketTest, TestConnectChallengeSendError) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResult(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - challenge reply receive fails
TEST_F(CastSocketTest, TestConnectChallengeReplyReceiveError) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResult(net::SYNCHRONOUS, net::ERR_FAILED);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test connection error - challenge reply verification fails
TEST_F(CastSocketTest, TestConnectChallengeVerificationFails) {
  CreateCastSocketSecure();
  SetupAuthMessage();

  socket_->SetupTcp1Connect(net::SYNCHRONOUS, net::OK);
  socket_->SetupSsl1Connect(net::SYNCHRONOUS, net::OK);
  socket_->AddWriteResultForMessage(net::ASYNC, auth_request_);
  socket_->AddReadResultForMessage(net::ASYNC, auth_reply_);
  socket_->AddReadResult(net::ASYNC, net::ERR_IO_PENDING);
  socket_->SetVerifyChallengeResult(false);

  EXPECT_CALL(handler_, OnConnectComplete(net::ERR_CONNECTION_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_CONNECT_ERROR,
                      A<const LastErrors&>()));
  socket_->Connect(base::Bind(&CompleteHandler::OnConnectComplete,
                              base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_CONNECT_ERROR, socket_->error_state());
}

// Test write success - single message (async)
TEST_F(CastSocketTest, TestWriteAsync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::ASYNC, test_proto_strs_[0]);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - single message (sync)
TEST_F(CastSocketTest, TestWriteSync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0]);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - single message sent in multiple chunks (async)
TEST_F(CastSocketTest, TestWriteChunkedAsync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::ASYNC, test_proto_strs_[0], 2);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - single message sent in multiple chunks (sync)
TEST_F(CastSocketTest, TestWriteChunkedSync) {
  CreateCastSocket();
  socket_->AddWriteResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0], 2);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(test_proto_strs_[0].size()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - multiple messages (async)
TEST_F(CastSocketTest, TestWriteManyAsync) {
  CreateCastSocket();
  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    size_t msg_size = test_proto_strs_[i].size();
    socket_->AddWriteResult(net::ASYNC, msg_size);
    EXPECT_CALL(handler_, OnWriteComplete(msg_size));
  }
  ConnectHelper();
  SetupAuthMessage();

  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    socket_->SendMessage(test_messages_[i],
                         base::Bind(&CompleteHandler::OnWriteComplete,
                                    base::Unretained(&handler_)));
  }
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write success - multiple messages (sync)
TEST_F(CastSocketTest, TestWriteManySync) {
  CreateCastSocket();
  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    size_t msg_size = test_proto_strs_[i].size();
    socket_->AddWriteResult(net::SYNCHRONOUS, msg_size);
    EXPECT_CALL(handler_, OnWriteComplete(msg_size));
  }
  ConnectHelper();
  SetupAuthMessage();

  for (size_t i = 0; i < arraysize(test_messages_); i++) {
    socket_->SendMessage(test_messages_[i],
                         base::Bind(&CompleteHandler::OnWriteComplete,
                                    base::Unretained(&handler_)));
  }
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write error - not connected
TEST_F(CastSocketTest, TestWriteErrorNotConnected) {
  CreateCastSocket();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));

  EXPECT_EQ(cast_channel::READY_STATE_NONE, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write error - very large message
TEST_F(CastSocketTest, TestWriteErrorLargeMessage) {
  CreateCastSocket();
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  size_t size = MessageFramer::MessageHeader::max_message_size() + 1;
  test_messages_[0].data.reset(
      new base::StringValue(std::string(size, 'a')));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test write error - network error (sync)
TEST_F(CastSocketTest, TestWriteNetworkErrorSync) {
  CreateCastSocket();
  socket_->AddWriteResult(net::SYNCHRONOUS, net::ERR_FAILED);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                      A<const LastErrors&>()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test write error - network error (async)
TEST_F(CastSocketTest, TestWriteErrorAsync) {
  CreateCastSocket();
  socket_->AddWriteResult(net::ASYNC, net::ERR_FAILED);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                      A<const LastErrors&>()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test write error - 0 bytes written should be considered an error
TEST_F(CastSocketTest, TestWriteErrorZeroBytesWritten) {
  CreateCastSocket();
  socket_->AddWriteResult(net::SYNCHRONOUS, 0);
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_FAILED));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                      A<const LastErrors&>()));
  socket_->SendMessage(test_messages_[0],
                       base::Bind(&CompleteHandler::OnWriteComplete,
                                  base::Unretained(&handler_)));
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test that when an error occurrs in one write, write callback is invoked for
// all pending writes with the error
TEST_F(CastSocketTest, TestWriteErrorWithMultiplePendingWritesAsync) {
  CreateCastSocket();
  socket_->AddWriteResult(net::ASYNC, net::ERR_SOCKET_NOT_CONNECTED);
  ConnectHelper();
  SetupAuthMessage();

  const int num_writes = arraysize(test_messages_);
  EXPECT_CALL(handler_, OnWriteComplete(net::ERR_SOCKET_NOT_CONNECTED))
      .Times(num_writes);
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                      A<const LastErrors&>()));
  for (int i = 0; i < num_writes; i++) {
    socket_->SendMessage(test_messages_[i],
                         base::Bind(&CompleteHandler::OnWriteComplete,
                                    base::Unretained(&handler_)));
  }
  RunPendingTasks();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test read success - single message (async)
TEST_F(CastSocketTest, TestReadAsync) {
  CreateCastSocket();
  socket_->AddReadResultForMessage(net::ASYNC, test_proto_strs_[0]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();
  SetupAuthMessage();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - single message (sync)
TEST_F(CastSocketTest, TestReadSync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->AddReadResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - single message received in multiple chunks (async)
TEST_F(CastSocketTest, TestReadChunkedAsync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->AddReadResultForMessage(net::ASYNC, test_proto_strs_[0], 2);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - single message received in multiple chunks (sync)
TEST_F(CastSocketTest, TestReadChunkedSync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->AddReadResultForMessage(net::SYNCHRONOUS, test_proto_strs_[0], 2);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - multiple messages (async)
TEST_F(CastSocketTest, TestReadManyAsync) {
  CreateCastSocket();
  SetupAuthMessage();
  size_t num_reads = arraysize(test_proto_strs_);
  for (size_t i = 0; i < num_reads; i++)
    socket_->AddReadResultForMessage(net::ASYNC, test_proto_strs_[i]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()))
      .Times(num_reads);
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read success - multiple messages (sync)
TEST_F(CastSocketTest, TestReadManySync) {
  CreateCastSocket();
  SetupAuthMessage();
  size_t num_reads = arraysize(test_proto_strs_);
  for (size_t i = 0; i < num_reads; i++)
    socket_->AddReadResultForMessage(net::SYNCHRONOUS, test_proto_strs_[i]);
  EXPECT_CALL(mock_delegate_,
              OnMessage(socket_.get(), A<const MessageInfo&>()))
      .Times(num_reads);
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_OPEN, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_NONE, socket_->error_state());
}

// Test read error - network error (async)
TEST_F(CastSocketTest, TestReadErrorAsync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->AddReadResult(net::ASYNC, net::ERR_SOCKET_NOT_CONNECTED);
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                      A<const LastErrors&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test read error - network error (sync)
TEST_F(CastSocketTest, TestReadErrorSync) {
  CreateCastSocket();
  SetupAuthMessage();
  socket_->AddReadResult(net::SYNCHRONOUS, net::ERR_SOCKET_NOT_CONNECTED);
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_SOCKET_ERROR,
                      A<const LastErrors&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_SOCKET_ERROR, socket_->error_state());
}

// Test read error - header parse error
TEST_F(CastSocketTest, TestReadHeaderParseError) {
  CreateCastSocket();
  SetupAuthMessage();

  uint32 body_size =
      base::HostToNet32(MessageFramer::MessageHeader::max_message_size() + 1);
  // TODO(munjal): Add a method to cast_message_util.h to serialize messages
  char header[sizeof(body_size)];
  memcpy(&header, &body_size, arraysize(header));
  socket_->AddReadResult(net::SYNCHRONOUS, header, arraysize(header));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_INVALID_MESSAGE,
                      A<const LastErrors&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE,
            socket_->error_state());
}

// Test read error - body parse error
TEST_F(CastSocketTest, TestReadBodyParseError) {
  CreateCastSocket();
  SetupAuthMessage();
  char body[] = "some body";
  uint32 body_size = base::HostToNet32(arraysize(body));
  char header[sizeof(body_size)];
  memcpy(&header, &body_size, arraysize(header));
  socket_->AddReadResult(net::SYNCHRONOUS, header, arraysize(header));
  socket_->AddReadResult(net::SYNCHRONOUS, body, arraysize(body));
  EXPECT_CALL(mock_delegate_,
              OnError(socket_.get(),
                      cast_channel::CHANNEL_ERROR_INVALID_MESSAGE,
                      A<const LastErrors&>()));
  ConnectHelper();

  EXPECT_EQ(cast_channel::READY_STATE_CLOSED, socket_->ready_state());
  EXPECT_EQ(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE,
            socket_->error_state());
}
}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
