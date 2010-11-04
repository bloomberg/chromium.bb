// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/state_matrix.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Return;

namespace media {

class MockStateHandler {
 public:
  enum State {
    A,
    B,
    C,
  };

  MOCK_METHOD0(HandleA, int());
  MOCK_METHOD0(HandleB, int());
  MOCK_METHOD0(HandleC, int());
};

TEST(StateMatrixTest, AddState) {
  MockStateHandler handler;
  StateMatrix sm;

  EXPECT_FALSE(sm.IsStateDefined(MockStateHandler::A));
  EXPECT_FALSE(sm.IsStateDefined(MockStateHandler::B));
  EXPECT_FALSE(sm.IsStateDefined(MockStateHandler::C));

  sm.AddState(MockStateHandler::A, &MockStateHandler::HandleA);

  EXPECT_TRUE(sm.IsStateDefined(MockStateHandler::A));
  EXPECT_FALSE(sm.IsStateDefined(MockStateHandler::B));
  EXPECT_FALSE(sm.IsStateDefined(MockStateHandler::C));
}

TEST(StateMatrixTest, ExecuteHandler) {
  MockStateHandler handler;
  StateMatrix sm;

  sm.AddState(MockStateHandler::A, &MockStateHandler::HandleA);

  EXPECT_CALL(handler, HandleA())
      .WillOnce(Return(MockStateHandler::B));

  EXPECT_EQ(MockStateHandler::B,
            sm.ExecuteHandler(&handler, MockStateHandler::A));
}

}  // namespace media
