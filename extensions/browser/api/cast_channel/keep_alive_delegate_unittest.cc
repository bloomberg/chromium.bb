// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/keep_alive_delegate.h"

#include "base/timer/mock_timer.h"
#include "extensions/browser/api/cast_channel/cast_test_util.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace extensions {
namespace core_api {
namespace cast_channel {
namespace {

const int64 kTestPingTimeoutMillis = 1000;
const int64 kTestLivenessTimeoutMillis = 10000;

// Extends MockTimer with a mockable method ResetTriggered() which permits
// test code to set GMock expectations for Timer::Reset().
class MockTimerWithMonitoredReset : public base::MockTimer {
 public:
  MockTimerWithMonitoredReset(bool retain_user_task, bool is_repeating)
      : base::MockTimer(retain_user_task, is_repeating) {}
  ~MockTimerWithMonitoredReset() override {}

  // Instrumentation point for determining how many times Reset() was called.
  MOCK_METHOD0(ResetTriggered, void(void));

  // Passes through the Reset call to the base MockTimer and visits the mock
  // ResetTriggered method.
  void Reset() override {
    base::MockTimer::Reset();
    ResetTriggered();
  }
};

class KeepAliveDelegateTest : public testing::Test {
 public:
  KeepAliveDelegateTest() {}
  ~KeepAliveDelegateTest() override {}

 protected:
  void SetUp() override {
    inner_delegate_ = new MockCastTransportDelegate;
    keep_alive_.reset(new KeepAliveDelegate(
        &socket_, make_scoped_ptr(inner_delegate_),
        base::TimeDelta::FromMilliseconds(kTestPingTimeoutMillis),
        base::TimeDelta::FromMilliseconds(kTestLivenessTimeoutMillis)));
    liveness_timer_ = new MockTimerWithMonitoredReset(true, false);
    ping_timer_ = new MockTimerWithMonitoredReset(true, false);
    keep_alive_->SetTimersForTest(make_scoped_ptr(ping_timer_),
                                  make_scoped_ptr(liveness_timer_));
  }

  MockCastSocket socket_;
  scoped_ptr<KeepAliveDelegate> keep_alive_;
  MockCastTransportDelegate* inner_delegate_;
  MockTimerWithMonitoredReset* liveness_timer_;
  MockTimerWithMonitoredReset* ping_timer_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveDelegateTest);
};

TEST_F(KeepAliveDelegateTest, TestPing) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(KeepAliveDelegate::CreateKeepAliveMessage(
                              KeepAliveDelegate::kHeartbeatPingType)),
                          _)).WillOnce(RunCompletionCallback<1>(net::OK));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);

  keep_alive_->Start();
  ping_timer_->Fire();
  keep_alive_->OnMessage(KeepAliveDelegate::CreateKeepAliveMessage(
      KeepAliveDelegate::kHeartbeatPongType));
}

TEST_F(KeepAliveDelegateTest, TestPingFailed) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(KeepAliveDelegate::CreateKeepAliveMessage(
                              KeepAliveDelegate::kHeartbeatPingType)),
                          _))
      .WillOnce(RunCompletionCallback<1>(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*inner_delegate_, OnError(CHANNEL_ERROR_SOCKET_ERROR, _));
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(1);

  keep_alive_->Start();
  ping_timer_->Fire();
}

TEST_F(KeepAliveDelegateTest, TestPingAndLivenessTimeout) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(KeepAliveDelegate::CreateKeepAliveMessage(
                              KeepAliveDelegate::kHeartbeatPingType)),
                          _)).WillOnce(RunCompletionCallback<1>(net::OK));
  EXPECT_CALL(*inner_delegate_, OnError(CHANNEL_ERROR_PING_TIMEOUT, _));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(1);

  keep_alive_->Start();
  ping_timer_->Fire();
  liveness_timer_->Fire();
}

TEST_F(KeepAliveDelegateTest, TestResetTimersAndPassthroughAllOtherTraffic) {
  CastMessage other_message =
      KeepAliveDelegate::CreateKeepAliveMessage("NEITHER_PING_NOR_PONG");

  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(other_message)));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);

  keep_alive_->Start();
  keep_alive_->OnMessage(other_message);
}

}  // namespace
}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
