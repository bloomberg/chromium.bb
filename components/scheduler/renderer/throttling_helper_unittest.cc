// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/throttling_helper.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "components/scheduler/renderer/web_view_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace scheduler {

class ThrottlingHelperTest : public testing::Test {
 public:
  ThrottlingHelperTest() {}
  ~ThrottlingHelperTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, make_scoped_ptr(new TestTimeSource(clock_.get())));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    throttling_helper_ = scheduler_->throttling_helper();
    timer_queue_ = scheduler_->NewTimerTaskRunner("test_queue");
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

 protected:
  scoped_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  scoped_ptr<RendererSchedulerImpl> scheduler_;
  scoped_refptr<TaskQueue> timer_queue_;
  ThrottlingHelper* throttling_helper_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(ThrottlingHelperTest);
};

TEST_F(ThrottlingHelperTest, DelayToNextRunTimeInSeconds) {
  EXPECT_EQ(base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.0)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.9),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.1)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.8),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.2)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.5),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.5)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.2),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.8)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.1),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.9)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.9),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.1)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.0)));

  EXPECT_EQ(base::TimeDelta::FromSecondsD(0.9),
            ThrottlingHelper::DelayToNextRunTimeInSeconds(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.1)));
}

namespace {
void TestTask(std::vector<base::TimeTicks>* run_times,
              base::SimpleTestTickClock* clock) {
  run_times->push_back(clock->NowTicks());
}
}  // namespace

TEST_F(ThrottlingHelperTest, TimerAlignment) {
  std::vector<base::TimeTicks> run_times;
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(800.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(1200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(8300.0));

  throttling_helper_->Throttle(timer_queue_.get());

  mock_task_runner_->RunUntilIdle();

  // Times are aligned to a multipple of 1000 milliseconds.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(9000.0)));
}

TEST_F(ThrottlingHelperTest, TimerAlignment_Unthrottled) {
  std::vector<base::TimeTicks> run_times;
  base::TimeTicks start_time = clock_->NowTicks();
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(800.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(1200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(8300.0));

  throttling_helper_->Throttle(timer_queue_.get());
  throttling_helper_->Unthrottle(timer_queue_.get());

  mock_task_runner_->RunUntilIdle();

  // Times are not aligned.
  EXPECT_THAT(
      run_times,
      ElementsAre(start_time + base::TimeDelta::FromMilliseconds(200.0),
                  start_time + base::TimeDelta::FromMilliseconds(800.0),
                  start_time + base::TimeDelta::FromMilliseconds(1200.0),
                  start_time + base::TimeDelta::FromMilliseconds(8300.0)));
}

TEST_F(ThrottlingHelperTest, WakeUpForNonDelayedTask) {
  std::vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  throttling_helper_->Throttle(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_queue_->PostTask(FROM_HERE,
                         base::Bind(&TestTask, &run_times, clock_.get()));

  mock_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(1000.0)));
}

TEST_F(ThrottlingHelperTest, WakeUpForDelayedTask) {
  std::vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  throttling_helper_->Throttle(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(1200.0));

  mock_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(2000.0)));
}

}  // namespace scheduler
