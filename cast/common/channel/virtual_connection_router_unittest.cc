// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/testing/fake_cast_socket.h"
#include "cast/common/channel/testing/mock_cast_message_handler.h"
#include "cast/common/channel/testing/mock_socket_error_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {
namespace {

using ::cast::channel::CastMessage;
using ::testing::_;
using ::testing::Invoke;

class VirtualConnectionRouterTest : public ::testing::Test {
 public:
  void SetUp() override {
    socket_ = fake_cast_socket_pair_.socket.get();
    router_.TakeSocket(&mock_error_handler_,
                       std::move(fake_cast_socket_pair_.socket));
  }

 protected:
  CastSocket& peer_socket() { return *fake_cast_socket_pair_.peer_socket; }

  FakeCastSocketPair fake_cast_socket_pair_;
  CastSocket* socket_;

  MockSocketErrorHandler mock_error_handler_;
  MockCastMessageHandler mock_message_handler_;

  VirtualConnectionManager manager_;
  VirtualConnectionRouter router_{&manager_};
};

}  // namespace

TEST_F(VirtualConnectionRouterTest, LocalIdHandler) {
  router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler_);
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-9873", socket_->socket_id()},
      {});

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(mock_message_handler_, OnMessage(_, socket_, _));
  EXPECT_TRUE(peer_socket().SendMessage(message).ok());

  EXPECT_CALL(mock_message_handler_, OnMessage(_, socket_, _));
  EXPECT_TRUE(peer_socket().SendMessage(message).ok());

  message.set_destination_id("receiver-4321");
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _, _)).Times(0);
  EXPECT_TRUE(peer_socket().SendMessage(message).ok());
}

TEST_F(VirtualConnectionRouterTest, RemoveLocalIdHandler) {
  router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler_);
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-9873", socket_->socket_id()},
      {});

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(mock_message_handler_, OnMessage(_, socket_, _));
  EXPECT_TRUE(peer_socket().SendMessage(message).ok());

  router_.RemoveHandlerForLocalId("receiver-1234");

  EXPECT_CALL(mock_message_handler_, OnMessage(_, socket_, _)).Times(0);
  EXPECT_TRUE(peer_socket().SendMessage(message).ok());
}

TEST_F(VirtualConnectionRouterTest, SendMessage) {
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-4321", socket_->socket_id()},
      {});

  CastMessage message;
  message.set_protocol_version(
      ::cast::channel::CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("receiver-1234");
  message.set_destination_id("sender-4321");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  EXPECT_CALL(fake_cast_socket_pair_.mock_peer_client, OnMessage(_, _))
      .WillOnce(Invoke([](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message.namespace_(), "zrqvn");
        EXPECT_EQ(message.source_id(), "receiver-1234");
        EXPECT_EQ(message.destination_id(), "sender-4321");
        ASSERT_EQ(message.payload_type(),
                  ::cast::channel::CastMessage_PayloadType_STRING);
        EXPECT_EQ(message.payload_utf8(), "cnlybnq");
      }));
  router_.SendMessage(
      VirtualConnection{"receiver-1234", "sender-4321", socket_->socket_id()},
      std::move(message));
}

TEST_F(VirtualConnectionRouterTest, CloseSocketRemovesVirtualConnections) {
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-4321", socket_->socket_id()},
      {});

  int id = socket_->socket_id();
  router_.CloseSocket(id);
  EXPECT_FALSE(manager_.GetConnectionData(
      VirtualConnection{"receiver-1234", "sender-4321", id}));
}

}  // namespace cast
}  // namespace openscreen
