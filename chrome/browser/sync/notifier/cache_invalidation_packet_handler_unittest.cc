// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"

#include "base/base64.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "google/cacheinvalidation/callback.h"
#include "google/cacheinvalidation/v2/system-resources.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "talk/base/task.h"
#include "talk/xmpp/asyncsocket.h"

namespace sync_notifier {

using ::testing::_;
using ::testing::Return;

class MockMessageCallback {
 public:
  void StoreMessage(const std::string& message) {
    last_message = message;
  }

  std::string last_message;
};

class CacheInvalidationPacketHandlerTest : public testing::Test {
 public:
  virtual ~CacheInvalidationPacketHandlerTest() {}
};

TEST_F(CacheInvalidationPacketHandlerTest, Basic) {
  MessageLoop message_loop;

  notifier::FakeBaseTask fake_base_task;

  std::string last_message;
  MockMessageCallback callback;
  invalidation::MessageCallback* mock_message_callback =
      invalidation::NewPermanentCallback(
          &callback, &MockMessageCallback::StoreMessage);

  const char kInboundMessage[] = "non-bogus";
  {
    CacheInvalidationPacketHandler handler(fake_base_task.AsWeakPtr());
    handler.SetMessageReceiver(mock_message_callback);

    // Take care of any tasks posted by the constructor.
    message_loop.RunAllPending();

    {
      handler.HandleInboundPacket("bogus");
      std::string inbound_message_encoded;
      EXPECT_TRUE(
          base::Base64Encode(kInboundMessage, &inbound_message_encoded));
      handler.HandleInboundPacket(inbound_message_encoded);
    }

    // Take care of any tasks posted by HandleOutboundPacket().
    message_loop.RunAllPending();

    EXPECT_EQ(callback.last_message, kInboundMessage);
  }
}

}  // namespace sync_notifier
