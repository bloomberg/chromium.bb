// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_client.h"

#include <memory>

#include "api/impl/quic/quic_service_common.h"
#include "api/impl/quic/testing/fake_quic_connection_factory.h"
#include "api/public/network_metrics.h"
#include "base/error.h"
#include "platform/api/logging.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

using ::testing::_;
using ::testing::Invoke;

class MockServiceObserver final : public ProtocolConnectionServiceObserver {
 public:
  ~MockServiceObserver() override = default;

  MOCK_METHOD0(OnRunning, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD1(OnMetrics, void(const NetworkMetrics& metrics));
  MOCK_METHOD1(OnError, void(const Error& error));
};

class MockMessageCallback final : public MessageDemuxer::MessageCallback {
 public:
  ~MockMessageCallback() override = default;

  MOCK_METHOD5(OnStreamMessage,
               ErrorOr<size_t>(uint64_t endpoint_id,
                               uint64_t connection_id,
                               msgs::Type message_type,
                               const uint8_t* buffer,
                               size_t buffer_size));
};

class MockConnectionObserver final : public ProtocolConnection::Observer {
 public:
  ~MockConnectionObserver() override = default;

  MOCK_METHOD1(OnConnectionChanged, void(const ProtocolConnection& connection));
  MOCK_METHOD1(OnConnectionClosed, void(const ProtocolConnection& connection));
};

class ConnectionCallback final
    : public ProtocolConnectionClient::ConnectionRequestCallback {
 public:
  explicit ConnectionCallback(std::unique_ptr<ProtocolConnection>* connection)
      : connection_(connection) {}
  ~ConnectionCallback() override = default;

  void OnConnectionOpened(
      uint64_t request_id,
      std::unique_ptr<ProtocolConnection>&& connection) override {
    OSP_DCHECK(!failed_ && !*connection_);
    *connection_ = std::move(connection);
  }

  void OnConnectionFailed(uint64_t request_id) override {
    OSP_DCHECK(!failed_ && !*connection_);
    failed_ = true;
  }

 private:
  bool failed_ = false;
  std::unique_ptr<ProtocolConnection>* const connection_;
};

class QuicClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto connection_factory = std::make_unique<FakeQuicConnectionFactory>(
        local_endpoint_, &server_demuxer_);
    connection_factory_ = connection_factory.get();
    client_ = std::make_unique<QuicClient>(
        &demuxer_, std::move(connection_factory), &mock_observer_);
  }

  void RunTasksUntilIdle() {
    do {
      client_->RunTasks();
    } while (!connection_factory_->idle());
  }

  void SendTestMessage(ProtocolConnection* connection) {
    MockMessageCallback mock_message_callback;
    MessageDemuxer::MessageWatch message_watch =
        server_demuxer_.WatchMessageType(
            0, msgs::Type::kPresentationConnectionMessage,
            &mock_message_callback);

    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.presentation_id = "some-id";
    message.connection_id = 7;
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from client");
    ASSERT_TRUE(msgs::EncodePresentationConnectionMessage(message, &buffer));
    connection->Write(buffer.data(), buffer.size());
    connection->CloseWriteEnd();

    ssize_t decode_result = 0;
    msgs::PresentationConnectionMessage received_message;
    EXPECT_CALL(
        mock_message_callback,
        OnStreamMessage(0, connection->connection_id(),
                        msgs::Type::kPresentationConnectionMessage, _, _))
        .WillOnce(Invoke([&decode_result, &received_message](
                             uint64_t endpoint_id, uint64_t connection_id,
                             msgs::Type message_type, const uint8_t* buffer,
                             size_t buffer_size) {
          decode_result = msgs::DecodePresentationConnectionMessage(
              buffer, buffer_size, &received_message);
          if (decode_result < 0)
            return ErrorOr<size_t>(Error::Code::kCborParsing);
          return ErrorOr<size_t>(decode_result);
        }));
    RunTasksUntilIdle();

    ASSERT_GT(decode_result, 0);
    EXPECT_EQ(decode_result, static_cast<ssize_t>(buffer.size() - 1));
    EXPECT_EQ(received_message.presentation_id, message.presentation_id);
    EXPECT_EQ(received_message.connection_id, message.connection_id);
    ASSERT_EQ(received_message.message.which,
              decltype(received_message.message.which)::kString);
    EXPECT_EQ(received_message.message.str, message.message.str);
  }

  const IPEndpoint local_endpoint_{{192, 168, 1, 10}, 44327};
  const IPEndpoint server_endpoint_{{192, 168, 1, 15}, 54368};
  MessageDemuxer demuxer_;
  MessageDemuxer server_demuxer_;
  FakeQuicConnectionFactory* connection_factory_;
  MockServiceObserver mock_observer_;
  std::unique_ptr<QuicClient> client_;
};

}  // namespace

TEST_F(QuicClientTest, Connect) {
  client_->Start();

  std::unique_ptr<ProtocolConnection> connection;
  ConnectionCallback connection_callback(&connection);
  ProtocolConnectionClient::ConnectRequest request =
      client_->Connect(server_endpoint_, &connection_callback);
  ASSERT_TRUE(request);

  RunTasksUntilIdle();
  ASSERT_TRUE(connection);

  SendTestMessage(connection.get());

  client_->Stop();
}

TEST_F(QuicClientTest, OpenImmediate) {
  client_->Start();

  std::unique_ptr<ProtocolConnection> connection1;
  std::unique_ptr<ProtocolConnection> connection2;

  connection2 = client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection2);

  ConnectionCallback connection_callback(&connection1);
  ProtocolConnectionClient::ConnectRequest request =
      client_->Connect(server_endpoint_, &connection_callback);
  ASSERT_TRUE(request);

  connection2 = client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection2);

  RunTasksUntilIdle();
  ASSERT_TRUE(connection1);

  connection2 = client_->CreateProtocolConnection(connection1->endpoint_id());
  ASSERT_TRUE(connection2);

  SendTestMessage(connection2.get());

  client_->Stop();
}

TEST_F(QuicClientTest, States) {
  std::unique_ptr<ProtocolConnection> connection1;
  ConnectionCallback connection_callback(&connection1);
  ProtocolConnectionClient::ConnectRequest request =
      client_->Connect(server_endpoint_, &connection_callback);
  EXPECT_FALSE(request);
  std::unique_ptr<ProtocolConnection> connection2 =
      client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection2);

  EXPECT_CALL(mock_observer_, OnRunning());
  EXPECT_TRUE(client_->Start());
  EXPECT_FALSE(client_->Start());

  request = client_->Connect(server_endpoint_, &connection_callback);
  ASSERT_TRUE(request);
  RunTasksUntilIdle();
  ASSERT_TRUE(connection1);
  MockConnectionObserver mock_connection_observer1;
  connection1->SetObserver(&mock_connection_observer1);

  connection2 = client_->CreateProtocolConnection(connection1->endpoint_id());
  ASSERT_TRUE(connection2);
  MockConnectionObserver mock_connection_observer2;
  connection2->SetObserver(&mock_connection_observer2);

  EXPECT_CALL(mock_connection_observer1, OnConnectionClosed(_));
  EXPECT_CALL(mock_connection_observer2, OnConnectionClosed(_));
  EXPECT_CALL(mock_observer_, OnStopped());
  EXPECT_TRUE(client_->Stop());
  EXPECT_FALSE(client_->Stop());

  request = client_->Connect(server_endpoint_, &connection_callback);
  EXPECT_FALSE(request);
  connection2 = client_->CreateProtocolConnection(1);
  EXPECT_FALSE(connection2);
}

}  // namespace openscreen
