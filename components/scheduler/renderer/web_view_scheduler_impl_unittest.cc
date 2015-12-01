// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_view_scheduler_impl.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

class WebViewSchedulerImplTest : public testing::Test {
 public:
  WebViewSchedulerImplTest() {}
  ~WebViewSchedulerImplTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delagate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, make_scoped_ptr(new TestTimeSource(clock_.get())));
    scheduler_.reset(new RendererSchedulerImpl(delagate_));
    web_view_scheduler_.reset(new WebViewSchedulerImpl(
        nullptr, scheduler_.get(), DisableBackgroundTimerThrottling()));
    web_frame_scheduler_ = web_view_scheduler_->createWebFrameSchedulerImpl();
  }

  void TearDown() override {
    web_frame_scheduler_.reset();
    web_view_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  virtual bool DisableBackgroundTimerThrottling() const { return false; }

  scoped_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delagate_;
  scoped_ptr<RendererSchedulerImpl> scheduler_;
  scoped_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  scoped_ptr<WebFrameSchedulerImpl> web_frame_scheduler_;
};

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersBefore) {
  blink::WebPassOwnPtr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->createFrameScheduler());
  blink::WebPassOwnPtr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->createFrameScheduler());
}

TEST_F(WebViewSchedulerImplTest, TestDestructionOfFrameSchedulersAfter) {
  blink::WebPassOwnPtr<blink::WebFrameScheduler> frame1(
      web_view_scheduler_->createFrameScheduler());
  blink::WebPassOwnPtr<blink::WebFrameScheduler> frame2(
      web_view_scheduler_->createFrameScheduler());
  web_view_scheduler_.reset();
}

namespace {
class RepeatingTask : public blink::WebTaskRunner::Task {
 public:
  RepeatingTask(blink::WebTaskRunner* web_task_runner, int* run_count)
      : web_task_runner_(web_task_runner), run_count_(run_count) {}

  ~RepeatingTask() override {}

  void run() override {
    (*run_count_)++;
    web_task_runner_->postDelayedTask(
        BLINK_FROM_HERE, new RepeatingTask(web_task_runner_, run_count_), 1.0);
  }

 private:
  blink::WebTaskRunner* web_task_runner_;  // NOT OWNED
  int* run_count_;                         // NOT OWNED
};
}  // namespace

TEST_F(WebViewSchedulerImplTest, RepeatingTimer_PageInForeground) {
  web_view_scheduler_->setPageInBackground(false);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

TEST_F(WebViewSchedulerImplTest, RepeatingTimer_PageInBackground) {
  web_view_scheduler_->setPageInBackground(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, run_count);
}

TEST_F(WebViewSchedulerImplTest, RepeatingLoadingTask_PageInBackground) {
  web_view_scheduler_->setPageInBackground(true);

  int run_count = 0;
  web_frame_scheduler_->loadingTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->loadingTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);  // Loading tasks should not be throttled
}

TEST_F(WebViewSchedulerImplTest, RepeatingTimers_OneBackgroundOneForeground) {
  scoped_ptr<WebViewSchedulerImpl> web_view_scheduler2(
      new WebViewSchedulerImpl(nullptr, scheduler_.get(), false));
  scoped_ptr<WebFrameSchedulerImpl> web_frame_scheduler2 =
      web_view_scheduler2->createWebFrameSchedulerImpl();

  web_view_scheduler_->setPageInBackground(false);
  web_view_scheduler2->setPageInBackground(true);

  int run_count1 = 0;
  int run_count2 = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count1),
      1.0);
  web_frame_scheduler2->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler2->timerTaskRunner(), &run_count2),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count1);
  EXPECT_EQ(1, run_count2);
}

class WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling
    : public WebViewSchedulerImplTest {
 public:
  WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling() {}
  ~WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling() override {}

  bool DisableBackgroundTimerThrottling() const override { return true; }
};

TEST_F(WebViewSchedulerImplTestWithDisabledBackgroundTimerThrottling,
       RepeatingTimer_PageInBackground) {
  web_view_scheduler_->setPageInBackground(true);

  int run_count = 0;
  web_frame_scheduler_->timerTaskRunner()->postDelayedTask(
      BLINK_FROM_HERE,
      new RepeatingTask(web_frame_scheduler_->timerTaskRunner(), &run_count),
      1.0);

  mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1000, run_count);
}

}  // namespace scheduler
