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
using testing::Mock;

namespace scheduler {

class MockTimeDomain : public TimeDomain {
 public:
  explicit MockTimeDomain(TimeDomain::Observer* observer)
      : TimeDomain(observer),
        now_(base::TimeTicks() + base::TimeDelta::FromSeconds(1)) {}

  ~MockTimeDomain() override {}

  using TimeDomain::NextScheduledRunTime;
  using TimeDomain::NextScheduledTaskQueue;
  using TimeDomain::ScheduleDelayedWork;
  using TimeDomain::UnregisterQueue;
  using TimeDomain::UpdateWorkQueues;
  using TimeDomain::RegisterAsUpdatableTaskQueue;

  // TimeSource implementation:
  LazyNow CreateLazyNow() override { return LazyNow(now_); }

  void AsValueIntoInternal(
      base::trace_event::TracedValue* state) const override {}

  bool MaybeAdvanceTime() override { return false; }
  const char* GetName() const override { return "Test"; }
  void OnRegisterWithTaskQueueManager(
      TaskQueueManagerDelegate* task_queue_manager_delegate,
      base::Closure do_work_closure) override {}

  MOCK_METHOD2(RequestWakeup, void(LazyNow* lazy_now, base::TimeDelta delay));

  void SetNow(base::TimeTicks now) { now_ = now; }

  base::TimeTicks Now() const { return now_; }

 private:
  base::TimeTicks now_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeDomain);
};

class TimeDomainTest : public testing::Test {
 public:
  void SetUp() final {
    time_domain_ = make_scoped_ptr(CreateMockTimeDomain());
    task_queue_ = make_scoped_refptr(new internal::TaskQueueImpl(
        nullptr, time_domain_.get(), TaskQueue::Spec("test_queue"),
        "test.category", "test.category"));
  }

  virtual MockTimeDomain* CreateMockTimeDomain() {
    return new MockTimeDomain(nullptr);
  }

  scoped_ptr<MockTimeDomain> time_domain_;
  scoped_refptr<internal::TaskQueueImpl> task_queue_;
};

TEST_F(TimeDomainTest, ScheduleDelayedWork) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks delayed_runtime = time_domain_->Now() + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, delay));
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

TEST_F(TimeDomainTest, RequestWakeup_OnlyCalledForEarlierTasks) {
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta delay4 = base::TimeDelta::FromMilliseconds(1);

  // RequestWakeup should always be called if there are no other wakeups.
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, delay1));
  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->ScheduleDelayedWork(task_queue_.get(),
                                    time_domain_->Now() + delay1, &lazy_now);

  Mock::VerifyAndClearExpectations(time_domain_.get());

  // RequestWakeup should not be called when scheduling later tasks.
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, _)).Times(0);
  time_domain_->ScheduleDelayedWork(task_queue_.get(),
                                    time_domain_->Now() + delay2, &lazy_now);
  time_domain_->ScheduleDelayedWork(task_queue_.get(),
                                    time_domain_->Now() + delay3, &lazy_now);

  // RequestWakeup should be called when scheduling earlier tasks.
  Mock::VerifyAndClearExpectations(time_domain_.get());
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, delay4));
  time_domain_->ScheduleDelayedWork(task_queue_.get(),
                                    time_domain_->Now() + delay4, &lazy_now);
}

TEST_F(TimeDomainTest, UnregisterQueue) {
  scoped_refptr<internal::TaskQueueImpl> task_queue2_ =
      make_scoped_refptr(new internal::TaskQueueImpl(
          nullptr, time_domain_.get(), TaskQueue::Spec("test_queue2"),
          "test.category", "test.category"));

  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, _)).Times(1);
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
  scoped_ptr<MockTimeDomain> dummy_delegate(new MockTimeDomain(nullptr));
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
  // next time MoveReadyDelayedTasksToDelayedWorkQueue is called, the task will
  // get moved onto the Incoming queue.
  base::TimeDelta dummy_delay = base::TimeDelta::FromMilliseconds(10);
  dummy_queue->PostDelayedTask(FROM_HERE, base::Closure(), dummy_delay);
  dummy_time_source.Advance(dummy_delay);

  // Now we can test that ScheduleDelayedWork triggers calls to
  // MoveReadyDelayedTasksToDelayedWorkQueue as expected.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(50);
  base::TimeTicks delayed_runtime = time_domain_->Now() + delay;
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, delay));
  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->ScheduleDelayedWork(dummy_queue.get(), delayed_runtime,
                                    &lazy_now);

  time_domain_->UpdateWorkQueues(false, nullptr);
  EXPECT_EQ(0UL, dummy_queue->DelayedWorkQueueSizeForTest());

  time_domain_->SetNow(delayed_runtime);
  time_domain_->UpdateWorkQueues(false, nullptr);
  EXPECT_EQ(1UL, dummy_queue->DelayedWorkQueueSizeForTest());
}

namespace {
class MockObserver : public TimeDomain::Observer {
 public:
  ~MockObserver() override {}

  MOCK_METHOD0(OnTimeDomainHasImmediateWork, void());
  MOCK_METHOD0(OnTimeDomainHasDelayedWork, void());
};
}  // namespace

class TimeDomainWithObserverTest : public TimeDomainTest {
 public:
  MockTimeDomain* CreateMockTimeDomain() override {
    observer_.reset(new MockObserver());
    return new MockTimeDomain(observer_.get());
  }

  scoped_ptr<MockObserver> observer_;
};

TEST_F(TimeDomainWithObserverTest, OnTimeDomainHasImmediateWork) {
  EXPECT_CALL(*observer_, OnTimeDomainHasImmediateWork());
  time_domain_->RegisterAsUpdatableTaskQueue(task_queue_.get());
}

TEST_F(TimeDomainWithObserverTest, OnTimeDomainHasDelayedWork) {
  EXPECT_CALL(*observer_, OnTimeDomainHasDelayedWork());
  EXPECT_CALL(*time_domain_.get(), RequestWakeup(_, _));
  LazyNow lazy_now = time_domain_->CreateLazyNow();
  time_domain_->ScheduleDelayedWork(
      task_queue_.get(),
      time_domain_->Now() + base::TimeDelta::FromMilliseconds(10), &lazy_now);
}

}  // namespace scheduler
