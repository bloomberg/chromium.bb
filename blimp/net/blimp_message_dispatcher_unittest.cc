// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Uniitest for data encryption functions.

#include "blimp/net/blimp_message_dispatcher.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Ref;
using testing::Return;

namespace blimp {

class MockReceiver : public BlimpMessageReceiver {
 public:
  MockReceiver() {}
  ~MockReceiver() override {}

  MOCK_METHOD1(OnBlimpMessage, net::Error(const BlimpMessage& message));
};

TEST(BlimpMessageDispatcherTest, AllInteractions) {
  BlimpMessage input_msg;
  BlimpMessage compositor_msg;
  input_msg.set_type(BlimpMessage::INPUT);
  compositor_msg.set_type(BlimpMessage::COMPOSITOR);

  MockReceiver receiver1;
  MockReceiver receiver2;
  EXPECT_CALL(receiver1, OnBlimpMessage(Ref(input_msg)))
      .WillOnce(Return(net::OK));
  EXPECT_CALL(receiver2, OnBlimpMessage(Ref(compositor_msg)))
      .WillOnce(Return(net::ERR_FAILED));

  BlimpMessageDispatcher dispatcher;
  dispatcher.AddReceiver(BlimpMessage::INPUT, &receiver1);
  dispatcher.AddReceiver(BlimpMessage::COMPOSITOR, &receiver2);
  EXPECT_EQ(net::OK, dispatcher.OnBlimpMessage(input_msg));
  EXPECT_EQ(net::ERR_FAILED, dispatcher.OnBlimpMessage(compositor_msg));
}

}  // namespace blimp
