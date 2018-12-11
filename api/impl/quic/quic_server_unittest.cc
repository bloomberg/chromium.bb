// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_server.h"

#include "api/impl/quic/testing/fake_quic_connection_factory.h"
#include "api/public/network_metrics.h"
#include "base/error.h"
#include "base/make_unique.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

using ::testing::_;
using ::testing::Invoke;

class MockServerObserver final : public ProtocolConnectionServer::Observer {
 public:
  ~MockServerObserver() override = default;

  MOCK_METHOD0(OnRunning, void());
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD1(OnMetrics, void(const NetworkMetrics& metrics));
  MOCK_METHOD1(OnError, void(const Error& error));

  MOCK_METHOD0(OnSuspended, void());

  void OnIncomingConnection(
      std::unique_ptr<ProtocolConnection>&& connection) override {
    OnIncomingConnectionMock(connection.release());
  }
  MOCK_METHOD1(OnIncomingConnectionMock, void(ProtocolConnection* connection));
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

class QuicServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto connection_factory = MakeUnique<FakeQuicConnectionFactory>(
        local_endpoint_, &client_demuxer_);
    connection_factory_ = connection_factory.get();
    ServerConfig config;
    config.connection_endpoints.push_back(local_endpoint_);
    server_ = MakeUnique<QuicServer>(
        config, &demuxer_, std::move(connection_factory), &mock_observer_);
  }

  void RunTasksUntilIdle() {
    do {
      server_->RunTasks();
    } while (!connection_factory_->idle());
  }

  void SendTestMessage(ProtocolConnection* connection) {
    MockMessageCallback mock_message_callback;
    MessageDemuxer::MessageWatch message_watch =
        client_demuxer_.WatchMessageType(
            0, msgs::Type::kPresentationConnectionMessage,
            &mock_message_callback);

    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.presentation_id = "some-id";
    message.connection_id = 7;
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from server");
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
  const IPEndpoint client_endpoint_{{192, 168, 1, 15}, 54368};
  MessageDemuxer demuxer_;
  MessageDemuxer client_demuxer_;
  FakeQuicConnectionFactory* connection_factory_;
  MockServerObserver mock_observer_;
  std::unique_ptr<QuicServer> server_;
};

}  // namespace

TEST_F(QuicServerTest, Connect) {
  server_->Start();

  std::unique_ptr<ProtocolConnection> connection;
  EXPECT_CALL(mock_observer_, OnIncomingConnectionMock(_))
      .WillOnce(Invoke(
          [&connection](ProtocolConnection* c) { connection.reset(c); }));
  connection_factory_->StartServerConnection(client_endpoint_);
  RunTasksUntilIdle();
  connection_factory_->StartIncomingStream(client_endpoint_);
  RunTasksUntilIdle();
  ASSERT_TRUE(connection);

  SendTestMessage(connection.get());

  server_->Stop();
}

TEST_F(QuicServerTest, OpenImmediate) {
  server_->Start();

  EXPECT_FALSE(server_->CreateProtocolConnection(1));

  std::unique_ptr<ProtocolConnection> connection1;
  EXPECT_CALL(mock_observer_, OnIncomingConnectionMock(_))
      .WillOnce(Invoke(
          [&connection1](ProtocolConnection* c) { connection1.reset(c); }));
  connection_factory_->StartServerConnection(client_endpoint_);
  RunTasksUntilIdle();
  connection_factory_->StartIncomingStream(client_endpoint_);
  RunTasksUntilIdle();
  ASSERT_TRUE(connection1);

  std::unique_ptr<ProtocolConnection> connection2;
  connection2 = server_->CreateProtocolConnection(connection1->endpoint_id());

  SendTestMessage(connection2.get());

  server_->Stop();
}

TEST_F(QuicServerTest, States) {
  EXPECT_CALL(mock_observer_, OnRunning());
  EXPECT_TRUE(server_->Start());
  EXPECT_FALSE(server_->Start());

  std::unique_ptr<ProtocolConnection> connection;
  EXPECT_CALL(mock_observer_, OnIncomingConnectionMock(_))
      .WillOnce(Invoke(
          [&connection](ProtocolConnection* c) { connection.reset(c); }));
  connection_factory_->StartServerConnection(client_endpoint_);
  RunTasksUntilIdle();
  connection_factory_->StartIncomingStream(client_endpoint_);
  RunTasksUntilIdle();
  ASSERT_TRUE(connection);
  MockConnectionObserver mock_connection_observer;
  connection->SetObserver(&mock_connection_observer);

  EXPECT_CALL(mock_connection_observer, OnConnectionClosed(_));
  EXPECT_CALL(mock_observer_, OnStopped());
  EXPECT_TRUE(server_->Stop());
  EXPECT_FALSE(server_->Stop());

  EXPECT_CALL(mock_observer_, OnRunning());
  EXPECT_TRUE(server_->Start());

  EXPECT_CALL(mock_observer_, OnSuspended());
  EXPECT_TRUE(server_->Suspend());
  EXPECT_FALSE(server_->Suspend());
  EXPECT_FALSE(server_->Start());

  EXPECT_CALL(mock_observer_, OnRunning());
  EXPECT_TRUE(server_->Resume());
  EXPECT_FALSE(server_->Resume());
  EXPECT_FALSE(server_->Start());

  EXPECT_CALL(mock_observer_, OnSuspended());
  EXPECT_TRUE(server_->Suspend());

  EXPECT_CALL(mock_observer_, OnStopped());
  EXPECT_TRUE(server_->Stop());
}

}  // namespace openscreen
