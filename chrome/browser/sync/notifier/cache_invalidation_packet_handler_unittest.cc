// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"

#include "base/base64.h"
#include "base/message_loop.h"
#include "base/weak_ptr.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "jingle/notifier/base/task_pump.h"
#include "jingle/notifier/base/weak_xmpp_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "talk/base/task.h"
#include "talk/xmpp/asyncsocket.h"

namespace sync_notifier {

using ::testing::_;
using ::testing::NotNull;
using ::testing::Return;

class MockNetworkEndpoint : public invalidation::NetworkEndpoint {
 public:
  MOCK_METHOD1(RegisterOutboundListener,
               void(invalidation::NetworkCallback*));
  MOCK_METHOD1(HandleInboundMessage, void(const invalidation::string&));
  MOCK_METHOD1(TakeOutboundMessage, void(invalidation::string*));
  MOCK_METHOD1(AdviseNetworkStatus, void(bool));
};

class MockInvalidationClient : public invalidation::InvalidationClient {
 public:
  MOCK_METHOD1(Start, void(const std::string& str));
  MOCK_METHOD1(Register, void(const invalidation::ObjectId&));
  MOCK_METHOD1(Unregister, void(const invalidation::ObjectId&));
  MOCK_METHOD0(network_endpoint, invalidation::NetworkEndpoint*());
};

class MockAsyncSocket : public buzz::AsyncSocket {
 public:
  virtual ~MockAsyncSocket() {}

  MOCK_METHOD0(state, State());
  MOCK_METHOD0(error, Error());
  MOCK_METHOD0(GetError, int());
  MOCK_METHOD1(Connect, bool(const talk_base::SocketAddress&));
  MOCK_METHOD3(Read, bool(char*, size_t, size_t*));
  MOCK_METHOD2(Write, bool(const char*, size_t));
  MOCK_METHOD0(Close, bool());
  MOCK_METHOD1(StartTls, bool(const std::string&));
};

class CacheInvalidationPacketHandlerTest : public testing::Test {
 public:
  virtual ~CacheInvalidationPacketHandlerTest() {}
};

TEST_F(CacheInvalidationPacketHandlerTest, Basic) {
  MessageLoop message_loop;

  notifier::TaskPump task_pump;
  // Owned by |task_pump|.
  notifier::WeakXmppClient* weak_xmpp_client =
      new notifier::WeakXmppClient(&task_pump);
  base::WeakPtr<talk_base::Task> base_task(weak_xmpp_client->AsWeakPtr());
  MockNetworkEndpoint mock_network_endpoint;
  MockInvalidationClient mock_invalidation_client;

  EXPECT_CALL(mock_invalidation_client, network_endpoint()).
      WillRepeatedly(Return(&mock_network_endpoint));

  EXPECT_CALL(mock_network_endpoint,
              RegisterOutboundListener(NotNull())).Times(1);
  EXPECT_CALL(mock_network_endpoint,
              RegisterOutboundListener(NULL)).Times(1);
  const char kInboundMessage[] = "non-bogus";
  EXPECT_CALL(mock_network_endpoint,
              HandleInboundMessage(kInboundMessage)).Times(1);
  EXPECT_CALL(mock_network_endpoint, TakeOutboundMessage(_)).Times(1);

  weak_xmpp_client->Start();
  buzz::XmppClientSettings settings;
  // Owned by |weak_xmpp_client|.
  MockAsyncSocket* mock_async_socket = new MockAsyncSocket();
  EXPECT_CALL(*mock_async_socket, Connect(_)).WillOnce(Return(true));
  weak_xmpp_client->Connect(settings, "en", mock_async_socket, NULL);
  // Initialize the XMPP client.
  message_loop.RunAllPending();

  {
    CacheInvalidationPacketHandler handler(
        base_task, &mock_invalidation_client);
    // Take care of any tasks posted by the constructor.
    message_loop.RunAllPending();

    {
      handler.HandleInboundPacket("bogus");
      std::string inbound_message_encoded;
      EXPECT_TRUE(
          base::Base64Encode(kInboundMessage, &inbound_message_encoded));
      handler.HandleInboundPacket(inbound_message_encoded);
    }

    handler.HandleOutboundPacket(&mock_network_endpoint);
    // Take care of any tasks posted by HandleOutboundPacket().
    message_loop.RunAllPending();
  }
}

}  // namespace sync_notifier
