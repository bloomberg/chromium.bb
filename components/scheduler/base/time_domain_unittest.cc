// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/time_domain.h"

#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/task_queue_manager_delegate_for_test.h"
#include "components/scheduler/base/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace scheduler {

class MockTimeDomain : public TimeDomain {
 public:
  MockTimeDomain()
      : now_(base::TimeTicks() + base::TimeDelta::FromSeconds(1)) {}

  using TimeDomain::NextScheduledRunTime;
  using TimeDomain::NextScheduledTaskQueue;
  using TimeDomain::ScheduleDelayedWork;
  using TimeDomain::UnregisterQueue;
  using TimeDomain::UpdateWorkQueues;

  // TimeSource implementation:
  LazyNow CreateLazyNow() override { return LazyNow(now_); }

  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override {}

  bool MaybeAdvanceTime() override { return false; }

  const char* GetName() const override { return "Test"; }

  MOCK_METHOD1(RequestWakeup, void(base::TimeDelta delay));

  void SetNow(base::TimeTicks now) { now_ = now; }

  base::TimeTicks Now() const { return now_; }

 private:
  base::TimeTicks now_;

  ~MockTimeDomain() override {}

  DISALLOW_COPY_AND_ASSIGN(MockTimeDomain);
};

class TimeDomainTest : public testing::Test {
 public:
  void SetUp() final {
    time_domain_ = make_scoped_refptr(new MockTimeDomain());
    task_queue_ = make_scoped_refptr(new internal::TaskQueueImpl(
        nullptr, time_domain_, TaskQueue::Spec("test_queue"), "test.category",
        "test.category"));
  }

  scoped_refptr<MockTimeDomain> time_domain_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
};

TEST_F(TimeDomainTest, ScheduleDelayedWork) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks delayed_runtime = time_domain_->Now() + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(delay));
  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->ScheduleDelayedWork(task_queue_.get(),
                                    time_domain_->Now() + delay, &lazy_now);

  base::TimeTicks next_scheduled_runtime;
  EXPECT_TRUE(time_domain_->NextScheduledRunTime(&next_scheduled_runtime));
  EXPECT_EQ(delayed_runtime, next_scheduled_runtime);

  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue_.get(), next_task_queue);
}

TEST_F(TimeDomainTest, UnregisterQueue) {
  scoped_refptr<internal::TaskQueueImpl> task_queue2_ =
      make_scoped_refptr(new internal::TaskQueueImpl(
          nullptr, time_domain_, TaskQueue::Spec("test_queue2"),
          "test.category", "test.category"));

  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_)).Times(2);
  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->ScheduleDelayedWork(
      task_queue_.get(),
      time_domain_->Now() + base::TimeDelta::FromMilliseconds(10), &lazy_now);
  time_domain_->ScheduleDelayedWork(
      task_queue2_.get(),
      time_domain_->Now() + base::TimeDelta::FromMilliseconds(100), &lazy_now);

  TaskQueue* next_task_queue;
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue_.get(), next_task_queue);

  time_domain_->UnregisterQueue(task_queue_.get());
  EXPECT_TRUE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
  EXPECT_EQ(task_queue2_.get(), next_task_queue);

  time_domain_->UnregisterQueue(task_queue2_.get());
  EXPECT_FALSE(time_domain_->NextScheduledTaskQueue(&next_task_queue));
}

TEST_F(TimeDomainTest, UpdateWorkQueues) {
  scoped_refptr<MockTimeDomain> dummy_delegate(new MockTimeDomain());
  base::SimpleTestTickClock dummy_time_source;
  scoped_refptr<cc::OrderedSimpleTaskRunner> dummy_task_runner(
      new cc::OrderedSimpleTaskRunner(&dummy_time_source, false));
  TaskQueueManager task_queue_manager(
      TaskQueueManagerDelegateForTest::Create(
          dummy_task_runner,
          make_scoped_ptr(new TestTimeSource(&dummy_time_source))),
      "test.scheduler", "test.scheduler", "scheduler.debug");
  scoped_refptr<internal::TaskQueueImpl> dummy_queue =
      task_queue_manager.NewTaskQueue(TaskQueue::Spec("test_queue"));

  // Post a delayed task on |dummy_queue| and advance the queue's clock so that
  // next time MoveReadyDelayedTasksToIncomingQueue is called, the task will
  // get moved onto the incomming queue.
  base::TimeDelta dummy_delay = base::TimeDelta::FromMilliseconds(10);
  dummy_queue->PostDelayedTask(FROM_HERE, base::Closure(), dummy_delay);
  dummy_time_source.Advance(dummy_delay);

  // Now we can test that ScheduleDelayedWork triggers calls to
  // MoveReadyDelayedTasksToIncomingQueue as expected.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(50);
  base::TimeTicks delayed_runtime = time_domain_->Now() + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(delay));
  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->ScheduleDelayedWork(dummy_queue.get(), delayed_runtime,
                                    &lazy_now);

  time_domain_->UpdateWorkQueues(false, nullptr);
  EXPECT_EQ(0UL, dummy_queue->IncomingQueueSizeForTest());

  time_domain_->SetNow(delayed_runtime);
  time_domain_->UpdateWorkQueues(false, nullptr);
  EXPECT_EQ(1UL, dummy_queue->IncomingQueueSizeForTest());
}

}  // namespace scheduler
