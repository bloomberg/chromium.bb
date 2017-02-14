// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/keep_alive_delegate.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "extensions/browser/api/cast_channel/cast_test_util.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Sequence;

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

const int64_t kTestPingTimeoutMillis = 1000;
const int64_t kTestLivenessTimeoutMillis = 10000;

// Extends MockTimer with a mockable method ResetTriggered() which permits
// test code to set GMock expectations for Timer::Reset().
class MockTimerWithMonitoredReset : public base::MockTimer {
 public:
  MockTimerWithMonitoredReset(bool retain_user_task, bool is_repeating)
      : base::MockTimer(retain_user_task, is_repeating) {}
  ~MockTimerWithMonitoredReset() override {}

  // Instrumentation point for determining how many times Reset() was called.
  MOCK_METHOD0(ResetTriggered, void(void));
  MOCK_METHOD0(Stop, void(void));

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
    logger_ = new Logger();
    keep_alive_.reset(new KeepAliveDelegate(
        &socket_, logger_, base::WrapUnique(inner_delegate_),
        base::TimeDelta::FromMilliseconds(kTestPingTimeoutMillis),
        base::TimeDelta::FromMilliseconds(kTestLivenessTimeoutMillis)));
    liveness_timer_ = new MockTimerWithMonitoredReset(true, false);
    ping_timer_ = new MockTimerWithMonitoredReset(true, false);
    EXPECT_CALL(*liveness_timer_, Stop()).Times(0);
    EXPECT_CALL(*ping_timer_, Stop()).Times(0);
    keep_alive_->SetTimersForTest(base::WrapUnique(ping_timer_),
                                  base::WrapUnique(liveness_timer_));
  }

  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  MockCastSocket socket_;
  std::unique_ptr<KeepAliveDelegate> keep_alive_;
  scoped_refptr<Logger> logger_;
  MockCastTransportDelegate* inner_delegate_;
  MockTimerWithMonitoredReset* liveness_timer_;
  MockTimerWithMonitoredReset* ping_timer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeepAliveDelegateTest);
};

TEST_F(KeepAliveDelegateTest, TestErrorHandledBeforeStarting) {
  keep_alive_->OnError(CHANNEL_ERROR_CONNECT_ERROR);
}

TEST_F(KeepAliveDelegateTest, TestPing) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(KeepAliveDelegate::CreateKeepAliveMessage(
                              KeepAliveDelegate::kHeartbeatPingType)),
                          _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*ping_timer_, Stop());

  keep_alive_->Start();
  ping_timer_->Fire();
  keep_alive_->OnMessage(KeepAliveDelegate::CreateKeepAliveMessage(
      KeepAliveDelegate::kHeartbeatPongType));
  RunPendingTasks();
}

TEST_F(KeepAliveDelegateTest, TestPingFailed) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(KeepAliveDelegate::CreateKeepAliveMessage(
                              KeepAliveDelegate::kHeartbeatPingType)),
                          _))
      .WillOnce(PostCompletionCallbackTask<1>(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*inner_delegate_, OnError(CHANNEL_ERROR_SOCKET_ERROR));
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, Stop());
  EXPECT_CALL(*ping_timer_, Stop()).Times(2);

  keep_alive_->Start();
  ping_timer_->Fire();
  RunPendingTasks();
  EXPECT_EQ(proto::PING_WRITE_ERROR,
            logger_->GetLastErrors(socket_.id()).event_type);
  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            logger_->GetLastErrors(socket_.id()).net_return_value);
}

TEST_F(KeepAliveDelegateTest, TestPingAndLivenessTimeout) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(KeepAliveDelegate::CreateKeepAliveMessage(
                              KeepAliveDelegate::kHeartbeatPingType)),
                          _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(*inner_delegate_, OnError(CHANNEL_ERROR_PING_TIMEOUT));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, Stop()).Times(2);
  EXPECT_CALL(*ping_timer_, Stop()).Times(2);

  keep_alive_->Start();
  ping_timer_->Fire();
  liveness_timer_->Fire();
  RunPendingTasks();
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
  RunPendingTasks();
}

TEST_F(KeepAliveDelegateTest, TestPassthroughMessagesAfterError) {
  CastMessage message =
      KeepAliveDelegate::CreateKeepAliveMessage("NEITHER_PING_NOR_PONG");
  CastMessage message_after_error =
      KeepAliveDelegate::CreateKeepAliveMessage("ANOTHER_NOT_PING_NOR_PONG");
  CastMessage late_ping_message = KeepAliveDelegate::CreateKeepAliveMessage(
      KeepAliveDelegate::kHeartbeatPingType);

  EXPECT_CALL(*inner_delegate_, Start()).Times(1);
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, Stop()).Times(1);
  EXPECT_CALL(*ping_timer_, Stop()).Times(1);

  Sequence message_and_error_sequence;
  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(message)))
      .Times(1)
      .InSequence(message_and_error_sequence)
      .RetiresOnSaturation();
  EXPECT_CALL(*inner_delegate_, OnError(CHANNEL_ERROR_INVALID_MESSAGE))
      .Times(1)
      .InSequence(message_and_error_sequence);
  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(message_after_error)))
      .Times(1)
      .InSequence(message_and_error_sequence)
      .RetiresOnSaturation();
  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(late_ping_message)))
      .Times(0)
      .InSequence(message_and_error_sequence)
      .RetiresOnSaturation();

  // Start, process one message, then error-out. KeepAliveDelegate will
  // automatically stop itself.
  keep_alive_->Start();
  keep_alive_->OnMessage(message);
  RunPendingTasks();
  keep_alive_->OnError(CHANNEL_ERROR_INVALID_MESSAGE);
  RunPendingTasks();

  // Process a non-PING/PONG message and expect it to pass through.
  keep_alive_->OnMessage(message_after_error);
  RunPendingTasks();

  // Process a late-arriving PING/PONG message, which should have no effect.
  keep_alive_->OnMessage(late_ping_message);
  RunPendingTasks();
}

}  // namespace
}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
