// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace page_load_metrics {

// IPC interceptor class, which we use to verify that certain IPC
// messages get sent.
class MockIPCInterceptor {
 public:
  void OnMessage(const IPC::Message& message) {
    IPC_BEGIN_MESSAGE_MAP(MockIPCInterceptor, message)
      IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
      IPC_MESSAGE_UNHANDLED(ADD_FAILURE())
    IPC_END_MESSAGE_MAP()
  }

  MOCK_METHOD2(OnTimingUpdated, void(PageLoadTiming, PageLoadMetadata));
};

// Implementation of the MetricsRenderFrameObserver class we're testing,
// with the GetTiming() and ShouldSendMetrics() methods stubbed out to make
// the rest of the class more testable.
class MockMetricsRenderFrameObserver : public MetricsRenderFrameObserver {
 public:
  MockMetricsRenderFrameObserver() : MetricsRenderFrameObserver(nullptr) {
    ON_CALL(*this, ShouldSendMetrics()).WillByDefault(Return(true));
    ON_CALL(*this, HasNoRenderFrame()).WillByDefault(Return(false));
  }

  std::unique_ptr<base::Timer> CreateTimer() const override {
    if (!mock_timer_)
      ADD_FAILURE() << "CreateTimer() called, but no MockTimer available.";
    return std::move(mock_timer_);
  }

  // We intercept sent messages and dispatch them to a MockIPCInterceptor, which
  // we use to verify that the expected IPC messages get sent.
  virtual bool Send(IPC::Message* message) {
    interceptor_.OnMessage(*message);
    delete message;
    return true;
  }

  void set_mock_timer(std::unique_ptr<base::Timer> timer) {
    ASSERT_EQ(nullptr, mock_timer_);
    mock_timer_ = std::move(timer);
  }

  MOCK_CONST_METHOD0(GetTiming, PageLoadTiming());
  MOCK_CONST_METHOD0(ShouldSendMetrics, bool());
  MOCK_CONST_METHOD0(HasNoRenderFrame, bool());
  MockIPCInterceptor* ipc_interceptor() { return &interceptor_; }

 private:
  StrictMock<MockIPCInterceptor> interceptor_;
  mutable std::unique_ptr<base::Timer> mock_timer_;
};

typedef testing::Test MetricsRenderFrameObserverTest;

TEST_F(MetricsRenderFrameObserverTest, NoMetrics) {
  NiceMock<MockMetricsRenderFrameObserver> observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(PageLoadTiming()));

  observer.DidChangePerformanceTiming();
  ASSERT_FALSE(mock_timer->IsRunning());
}

TEST_F(MetricsRenderFrameObserverTest, SingleMetric) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(10);

  NiceMock<MockMetricsRenderFrameObserver> observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));
  observer.DidCommitProvisionalLoad(true, false);
  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));
  mock_timer->Fire();

  timing.first_layout = first_layout;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));

  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));

  observer.DidChangePerformanceTiming();
  mock_timer->Fire();
}

TEST_F(MetricsRenderFrameObserverTest, MultipleMetrics) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta dom_event = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(2);

  NiceMock<MockMetricsRenderFrameObserver> observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));
  observer.DidCommitProvisionalLoad(true, false);
  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));
  mock_timer->Fire();

  timing.first_layout = first_layout;
  timing.dom_content_loaded_event_start = dom_event;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));

  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));
  observer.DidChangePerformanceTiming();
  mock_timer->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  testing::Mock::VerifyAndClearExpectations(observer.ipc_interceptor());

  timing.load_event_start = load_event;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));

  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));
  observer.DidChangePerformanceTiming();
  mock_timer->Fire();

  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  testing::Mock::VerifyAndClearExpectations(observer.ipc_interceptor());

  // The PageLoadTiming above includes timing information for the first layout,
  // dom content, and load metrics. However, since we've already generated
  // timing information for all of these metrics previously, we do not expect
  // this invocation to generate any additional metrics.
  observer.DidChangePerformanceTiming();
  ASSERT_FALSE(mock_timer->IsRunning());
}

TEST_F(MetricsRenderFrameObserverTest, MultipleNavigations) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta dom_event = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(2);

  NiceMock<MockMetricsRenderFrameObserver> observer;
  base::MockTimer* mock_timer = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer));

  PageLoadTiming timing;
  timing.navigation_start = nav_start;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));
  observer.DidCommitProvisionalLoad(true, false);
  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));
  mock_timer->Fire();

  timing.first_layout = first_layout;
  timing.dom_content_loaded_event_start = dom_event;
  timing.load_event_start = load_event;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing));
  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing, PageLoadMetadata()));
  observer.DidChangePerformanceTiming();
  mock_timer->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  testing::Mock::VerifyAndClearExpectations(observer.ipc_interceptor());

  base::Time nav_start_2 = base::Time::FromDoubleT(100);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMillisecondsD(20);
  base::TimeDelta dom_event_2 = base::TimeDelta::FromMillisecondsD(20);
  base::TimeDelta load_event_2 = base::TimeDelta::FromMillisecondsD(20);
  PageLoadTiming timing_2;
  timing_2.navigation_start = nav_start_2;

  base::MockTimer* mock_timer2 = new base::MockTimer(false, false);
  observer.set_mock_timer(base::WrapUnique(mock_timer2));

  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing_2));
  observer.DidCommitProvisionalLoad(true, false);
  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing_2, PageLoadMetadata()));
  mock_timer2->Fire();

  timing_2.first_layout = first_layout_2;
  timing_2.dom_content_loaded_event_start = dom_event_2;
  timing_2.load_event_start = load_event_2;
  EXPECT_CALL(observer, GetTiming()).WillRepeatedly(Return(timing_2));

  EXPECT_CALL(*observer.ipc_interceptor(),
              OnTimingUpdated(timing_2, PageLoadMetadata()));
  observer.DidChangePerformanceTiming();
  mock_timer2->Fire();
}

}  // namespace page_load_metrics
