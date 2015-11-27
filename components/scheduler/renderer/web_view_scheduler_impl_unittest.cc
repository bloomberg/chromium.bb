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

namespace scheduler {

class WebViewSchedulerImplTest : public testing::Test {
 public:
  WebViewSchedulerImplTest() {}
  ~WebViewSchedulerImplTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(clock_.get(), false));
    main_task_runner_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, make_scoped_ptr(new TestTimeSource(clock_.get())));
    scheduler_.reset(new RendererSchedulerImpl(main_task_runner_));
    web_view_scheduler_.reset(
        new WebViewSchedulerImpl(nullptr, scheduler_.get()));
  }

  void TearDown() override { scheduler_->Shutdown(); }

  scoped_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  scoped_ptr<RendererSchedulerImpl> scheduler_;
  scoped_ptr<WebViewSchedulerImpl> web_view_scheduler_;
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

}  // namespace scheduler
