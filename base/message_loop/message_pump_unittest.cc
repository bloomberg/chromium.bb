// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace base {
namespace {

class MockMessagePumpDelegate : public MessagePump::Delegate {
 public:
  MockMessagePumpDelegate() = default;

  // MessagePump::Delegate:
  MOCK_METHOD0(DoWork, bool());
  MOCK_METHOD1(DoDelayedWork, bool(TimeTicks*));
  MOCK_METHOD0(DoIdleWork, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMessagePumpDelegate);
};

class MessagePumpTest : public ::testing::TestWithParam<MessageLoopBase::Type> {
 public:
  MessagePumpTest()
      : message_pump_(MessageLoop::CreateMessagePumpForType(GetParam())) {}

 protected:
  std::unique_ptr<MessagePump> message_pump_;
};

TEST_P(MessagePumpTest, QuitStopsWork) {
  testing::StrictMock<MockMessagePumpDelegate> delegate;

  // Not expecting any calls to DoDelayedWork or DoIdleWork after quitting.
  EXPECT_CALL(delegate, DoWork).WillOnce(Invoke([this] {
    message_pump_->Quit();
    return false;
  }));
  EXPECT_CALL(delegate, DoDelayedWork(_)).Times(0);
  EXPECT_CALL(delegate, DoIdleWork()).Times(0);

  message_pump_->ScheduleWork();
  message_pump_->Run(&delegate);
}

INSTANTIATE_TEST_CASE_P(,
                        MessagePumpTest,
                        ::testing::Values(MessageLoop::TYPE_DEFAULT,
                                          MessageLoop::TYPE_UI,
                                          MessageLoop::TYPE_IO));

}  // namespace
}  // namespace base