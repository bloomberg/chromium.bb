// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/cast_socket.h"

#include "cast/common/channel/message_framer.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_task_runner.h"

namespace cast {
namespace channel {
namespace {

using openscreen::ErrorOr;
using openscreen::IPEndpoint;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::TaskRunner;
using openscreen::platform::TlsConnection;

using ::testing::_;
using ::testing::Invoke;

class MockTlsConnection final : public TlsConnection {
 public:
  MockTlsConnection(TaskRunner* task_runner,
                    IPEndpoint local_address,
                    IPEndpoint remote_address)
      : TlsConnection(task_runner),
        local_address_(local_address),
        remote_address_(remote_address) {}

  ~MockTlsConnection() override = default;

  MOCK_METHOD(void, Write, (const void* data, size_t len));

  IPEndpoint local_address() const override { return local_address_; }
  IPEndpoint remote_address() const override { return remote_address_; }

  void OnWriteBlocked() { TlsConnection::OnWriteBlocked(); }
  void OnWriteUnblocked() { TlsConnection::OnWriteUnblocked(); }
  void OnError(Error error) { TlsConnection::OnError(error); }
  void OnRead(std::vector<uint8_t> block) { TlsConnection::OnRead(block); }

 private:
  const IPEndpoint local_address_;
  const IPEndpoint remote_address_;
};

class MockCastSocketClient final : public CastSocket::Client {
 public:
  ~MockCastSocketClient() override = default;

  MOCK_METHOD(void, OnError, (CastSocket * socket, Error error));
  MOCK_METHOD(void, OnMessage, (CastSocket * socket, CastMessage message));
};

class CastSocketTest : public ::testing::Test {
 public:
  void SetUp() override {
    message_.set_protocol_version(CastMessage::CASTV2_1_0);
    message_.set_source_id("source");
    message_.set_destination_id("destination");
    message_.set_namespace_("namespace");
    message_.set_payload_type(CastMessage::STRING);
    message_.set_payload_utf8("payload");
    ErrorOr<std::vector<uint8_t>> serialized_or_error =
        message_serialization::Serialize(message_);
    ASSERT_TRUE(serialized_or_error);
    frame_serial_ = std::move(serialized_or_error.value());
  }

 protected:
  FakeClock clock_{openscreen::platform::Clock::now()};
  FakeTaskRunner task_runner_{&clock_};
  IPEndpoint local_{{10, 0, 1, 7}, 1234};
  IPEndpoint remote_{{10, 0, 1, 9}, 4321};
  std::unique_ptr<MockTlsConnection> moved_connection_{
      new MockTlsConnection(&task_runner_, local_, remote_)};
  MockTlsConnection* connection_{moved_connection_.get()};
  MockCastSocketClient mock_client_;
  CastSocket socket_{std::move(moved_connection_), &mock_client_, 1};
  CastMessage message_;
  std::vector<uint8_t> frame_serial_;
};

}  // namespace

TEST_F(CastSocketTest, SendMessage) {
  EXPECT_CALL(*connection_, Write(_, _))
      .WillOnce(Invoke([this](const void* data, size_t len) {
        EXPECT_EQ(
            frame_serial_,
            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                 reinterpret_cast<const uint8_t*>(data) + len));
      }));
  ASSERT_TRUE(socket_.SendMessage(message_).ok());
}

TEST_F(CastSocketTest, ReadCompleteMessage) {
  const uint8_t* data = frame_serial_.data();
  EXPECT_CALL(mock_client_, OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection_->OnRead(std::vector<uint8_t>(data, data + frame_serial_.size()));
  task_runner_.RunTasksUntilIdle();
}

TEST_F(CastSocketTest, ReadChunkedMessage) {
  const uint8_t* data = frame_serial_.data();
  EXPECT_CALL(mock_client_, OnMessage(_, _)).Times(0);
  connection_->OnRead(std::vector<uint8_t>(data, data + 10));
  task_runner_.RunTasksUntilIdle();

  EXPECT_CALL(mock_client_, OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection_->OnRead(
      std::vector<uint8_t>(data + 10, data + frame_serial_.size()));
  task_runner_.RunTasksUntilIdle();

  std::vector<uint8_t> double_message;
  double_message.insert(double_message.end(), frame_serial_.begin(),
                        frame_serial_.end());
  double_message.insert(double_message.end(), frame_serial_.begin(),
                        frame_serial_.end());
  data = double_message.data();
  EXPECT_CALL(mock_client_, OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection_->OnRead(
      std::vector<uint8_t>(data, data + frame_serial_.size() + 10));
  task_runner_.RunTasksUntilIdle();

  EXPECT_CALL(mock_client_, OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection_->OnRead(std::vector<uint8_t>(data + frame_serial_.size() + 10,
                                           data + double_message.size()));
  task_runner_.RunTasksUntilIdle();
}

TEST_F(CastSocketTest, SendMessageWhileBlocked) {
  connection_->OnWriteBlocked();
  task_runner_.RunTasksUntilIdle();
  EXPECT_CALL(*connection_, Write(_, _)).Times(0);
  ASSERT_TRUE(socket_.SendMessage(message_).ok());

  EXPECT_CALL(*connection_, Write(_, _))
      .WillOnce(Invoke([this](const void* data, size_t len) {
        EXPECT_EQ(
            frame_serial_,
            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                 reinterpret_cast<const uint8_t*>(data) + len));
      }));
  connection_->OnWriteUnblocked();
  task_runner_.RunTasksUntilIdle();

  EXPECT_CALL(*connection_, Write(_, _)).Times(0);
  connection_->OnWriteBlocked();
  task_runner_.RunTasksUntilIdle();
  connection_->OnWriteUnblocked();
  task_runner_.RunTasksUntilIdle();
}

TEST_F(CastSocketTest, ErrorWhileEmptyingQueue) {
  connection_->OnWriteBlocked();
  task_runner_.RunTasksUntilIdle();
  EXPECT_CALL(*connection_, Write(_, _)).Times(0);
  ASSERT_TRUE(socket_.SendMessage(message_).ok());

  EXPECT_CALL(*connection_, Write(_, _))
      .WillOnce(Invoke([this](const void* data, size_t len) {
        EXPECT_EQ(
            frame_serial_,
            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                 reinterpret_cast<const uint8_t*>(data) + len));
        connection_->OnError(Error::Code::kUnknownError);
      }));
  connection_->OnWriteUnblocked();
  task_runner_.RunTasksUntilIdle();

  EXPECT_CALL(*connection_, Write(_, _)).Times(0);
  ASSERT_FALSE(socket_.SendMessage(message_).ok());
}

}  // namespace channel
}  // namespace cast
