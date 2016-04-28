// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

class MockIPCSender : public IPC::Sender {
 public:
  bool Send(IPC::Message* message) {
    IPC_BEGIN_MESSAGE_MAP(MockIPCSender, *message)
      IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
      IPC_MESSAGE_UNHANDLED(ADD_FAILURE())
    IPC_END_MESSAGE_MAP()

    delete message;
    return true;
  }

  MOCK_METHOD2(OnTimingUpdated, void(PageLoadTiming, PageLoadMetadata));
};

// Thin wrapper around PageTimingMetricsSender that provides access to the
// MockTimer instance.
class TestPageTimingMetricsSender : public PageTimingMetricsSender {
 public:
  explicit TestPageTimingMetricsSender(IPC::Sender* ipc_sender,
                                       const PageLoadTiming& initial_timing)
      : PageTimingMetricsSender(
            ipc_sender,
            MSG_ROUTING_NONE,
            std::unique_ptr<base::Timer>(new base::MockTimer(false, false)),
            initial_timing) {}

  base::MockTimer* mock_timer() const {
    return reinterpret_cast<base::MockTimer*>(timer());
  }
};

class PageTimingMetricsSenderTest : public testing::Test {
 public:
  PageTimingMetricsSenderTest()
      : metrics_sender_(&mock_ipc_sender_, PageLoadTiming()) {}

 protected:
  testing::StrictMock<MockIPCSender> mock_ipc_sender_;
  TestPageTimingMetricsSender metrics_sender_;
};

TEST_F(PageTimingMetricsSenderTest, Basic) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  timing.first_layout = first_layout;

  metrics_sender_.Send(timing);

  // Firing the timer should trigger sending of an OnTimingUpdated IPC.
  EXPECT_CALL(mock_ipc_sender_, OnTimingUpdated(timing, PageLoadMetadata()));
  ASSERT_TRUE(metrics_sender_.mock_timer()->IsRunning());
  metrics_sender_.mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_.mock_timer()->IsRunning());

  // At this point, we should have triggered the send of the PageLoadTiming IPC.
  testing::Mock::VerifyAndClearExpectations(&mock_ipc_sender_);

  // Attempt to send the same timing instance again. The send should be
  // suppressed, since the timing instance hasn't changed since the last send.
  metrics_sender_.Send(timing);
  EXPECT_FALSE(metrics_sender_.mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, CoalesceMultipleIPCs) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(4);

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  timing.first_layout = first_layout;

  metrics_sender_.Send(timing);
  ASSERT_TRUE(metrics_sender_.mock_timer()->IsRunning());

  // Send an updated PageLoadTiming before the timer has fired. When the timer
  // fires, the updated PageLoadTiming should be sent.
  timing.load_event_start = load_event;
  metrics_sender_.Send(timing);

  // Firing the timer should trigger sending of the OnTimingUpdated IPC with
  // the most recently provided PageLoadTiming instance.
  EXPECT_CALL(mock_ipc_sender_, OnTimingUpdated(timing, PageLoadMetadata()));
  metrics_sender_.mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_.mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, MultipleIPCs) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(4);

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  timing.first_layout = first_layout;

  metrics_sender_.Send(timing);
  ASSERT_TRUE(metrics_sender_.mock_timer()->IsRunning());
  EXPECT_CALL(mock_ipc_sender_, OnTimingUpdated(timing, PageLoadMetadata()));
  metrics_sender_.mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_.mock_timer()->IsRunning());
  testing::Mock::VerifyAndClearExpectations(&mock_ipc_sender_);

  // Send an updated PageLoadTiming after the timer for the first send request
  // has fired, and verify that a second IPC is sent.
  timing.load_event_start = load_event;
  metrics_sender_.Send(timing);
  ASSERT_TRUE(metrics_sender_.mock_timer()->IsRunning());
  EXPECT_CALL(mock_ipc_sender_, OnTimingUpdated(timing, PageLoadMetadata()));
  metrics_sender_.mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_.mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, SendIPCOnDestructor) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);
  timing.first_layout = base::TimeDelta::FromMilliseconds(10);

  // This test wants to verify behavior in the PageTimingMetricsSender
  // destructor, the EXPECT_CALL will be verified when the test tears down and
  // |metrics_sender_| goes out of scope.
  metrics_sender_.Send(timing);
  EXPECT_CALL(mock_ipc_sender_, OnTimingUpdated(timing, PageLoadMetadata()));
  ASSERT_TRUE(metrics_sender_.mock_timer()->IsRunning());
}

}  // namespace page_load_metrics
