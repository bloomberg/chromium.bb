// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"

#include "base/base64.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "talk/base/task.h"
#include "talk/xmpp/asyncsocket.h"

namespace sync_notifier {

using ::testing::_;
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

class CacheInvalidationPacketHandlerTest : public testing::Test {
 public:
  virtual ~CacheInvalidationPacketHandlerTest() {}
};

TEST_F(CacheInvalidationPacketHandlerTest, Basic) {
  MessageLoop message_loop;

  notifier::FakeBaseTask fake_base_task;

  MockNetworkEndpoint mock_network_endpoint;
  MockInvalidationClient mock_invalidation_client;

  EXPECT_CALL(mock_invalidation_client, network_endpoint()).
      WillRepeatedly(Return(&mock_network_endpoint));

  const char kInboundMessage[] = "non-bogus";
  EXPECT_CALL(mock_network_endpoint,
              HandleInboundMessage(kInboundMessage)).Times(1);
  EXPECT_CALL(mock_network_endpoint, TakeOutboundMessage(_)).Times(1);

  {
    CacheInvalidationPacketHandler handler(
        fake_base_task.AsWeakPtr(), &mock_invalidation_client);
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
