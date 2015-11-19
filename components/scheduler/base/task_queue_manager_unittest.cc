// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_manager.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_manager_delegate_for_test.h"
#include "components/scheduler/base/task_queue_selector.h"
#include "components/scheduler/base/task_queue_sets.h"
#include "components/scheduler/base/test_always_fail_time_source.h"
#include "components/scheduler/base/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::_;

namespace scheduler {

class MessageLoopTaskRunner : public TaskQueueManagerDelegateForTest {
 public:
  static scoped_refptr<MessageLoopTaskRunner> Create(
      scoped_ptr<base::TickClock> tick_clock) {
    return make_scoped_refptr(new MessageLoopTaskRunner(tick_clock.Pass()));
  }

  // NestableTaskRunner implementation.
  bool IsNested() const override {
    return base::MessageLoop::current()->IsNested();
  }

 private:
  explicit MessageLoopTaskRunner(scoped_ptr<base::TickClock> tick_clock)
      : TaskQueueManagerDelegateForTest(base::MessageLoop::current()
                                            ->task_runner(),
                                        tick_clock.Pass()) {}
  ~MessageLoopTaskRunner() override {}
};

class TaskQueueManagerTest : public testing::Test {
 public:
  void DeleteTaskQueueManager() { manager_.reset(); }

 protected:
  void InitializeWithClock(size_t num_queues,
                           scoped_ptr<base::TickClock> test_time_source) {
    test_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(now_src_.get(), false));
    main_task_runner_ = TaskQueueManagerDelegateForTest::Create(
        test_task_runner_.get(),
        make_scoped_ptr(new TestTimeSource(now_src_.get())));
    manager_ = make_scoped_ptr(new TaskQueueManager(
        main_task_runner_, "test.scheduler", "test.scheduler",
        "test.scheduler.debug"));

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));
  }

  void Initialize(size_t num_queues) {
    now_src_.reset(new base::SimpleTestTickClock());
    now_src_->Advance(base::TimeDelta::FromMicroseconds(1000));
    InitializeWithClock(num_queues,
                        make_scoped_ptr(new TestTimeSource(now_src_.get())));
  }

  void InitializeWithRealMessageLoop(size_t num_queues) {
    message_loop_.reset(new base::MessageLoop());
    manager_ = make_scoped_ptr(new TaskQueueManager(
        MessageLoopTaskRunner::Create(
            make_scoped_ptr(new TestTimeSource(now_src_.get()))),
        "test.scheduler", "test.scheduler", "test.scheduler.debug"));

    for (size_t i = 0; i < num_queues; i++)
      runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));
  }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<base::SimpleTestTickClock> now_src_;
  scoped_refptr<TaskQueueManagerDelegateForTest> main_task_runner_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner_;
  scoped_ptr<TaskQueueManager> manager_;
  std::vector<scoped_refptr<internal::TaskQueueImpl>> runners_;
};

void PostFromNestedRunloop(base::MessageLoop* message_loop,
                           base::SingleThreadTaskRunner* runner,
                           std::vector<std::pair<base::Closure, bool>>* tasks) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  for (std::pair<base::Closure, bool>& pair : *tasks) {
    if (pair.second) {
      runner->PostTask(FROM_HERE, pair.first);
    } else {
      runner->PostNonNestableTask(FROM_HERE, pair.first);
    }
  }
  message_loop->RunUntilIdle();
}

void NopTask() {}

TEST_F(TaskQueueManagerTest, NowNotCalledWhenThereAreNoDelayedTasks) {
  message_loop_.reset(new base::MessageLoop());
  manager_ = make_scoped_ptr(new TaskQueueManager(
      MessageLoopTaskRunner::Create(
          make_scoped_ptr(new TestAlwaysFailTimeSource())),
      "test.scheduler", "test.scheduler", "test.scheduler.debug"));

  for (size_t i = 0; i < 3; i++)
    runners_.push_back(manager_->NewTaskQueue(TaskQueue::Spec("test_queue")));

  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&NopTask));

  message_loop_->RunUntilIdle();
}

void NullTask() {}

void TestTask(int value, std::vector<int>* out_result) {
  out_result->push_back(value);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));
  runners_[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 1, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners_[0]->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 5, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 4, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 5, &run_order), true));

  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&PostFromNestedRunloop, message_loop_.get(), runners_[0],
                 base::Unretained(&tasks_to_post_from_nested_loop)));

  message_loop_->RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1, 2, 4, 5, 3));
}

TEST_F(TaskQueueManagerTest, QueuePolling) {
  Initialize(1u);

  std::vector<int> run_order;
  EXPECT_TRUE(runners_[0]->IsQueueEmpty());
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(runners_[0]->IsQueueEmpty());

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(runners_[0]->IsQueueEmpty());
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u);

  std::vector<int> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  EXPECT_EQ(delay, test_task_runner_->DelayToNextTaskTime());
  EXPECT_TRUE(runners_[0]->IsQueueEmpty());
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
}

bool MessageLoopTaskCounter(size_t* count) {
  *count = *count + 1;
  return true;
}

TEST_F(TaskQueueManagerTest, DelayedTaskExecutedInOneMessageLoopTask) {
  Initialize(1u);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  test_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));
  EXPECT_EQ(1u, task_count);
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(8));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(3));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(3),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(3));
  EXPECT_THAT(run_order, ElementsAre(3, 2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(2));
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_AscendingOrder) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(1));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(5));

  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(4),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(4));
  EXPECT_THAT(run_order, ElementsAre(1, 2));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(5),
            test_task_runner_->DelayToNextTaskTime());

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, PostDelayedTask_SharesUnderlyingDelayedTasks) {
  Initialize(1u);

  std::vector<int> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               delay);

  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
}

class TestObject {
 public:
  ~TestObject() { destructor_count_++; }

  void Run() { FAIL() << "TestObject::Run should not be called"; }

  static int destructor_count_;
};

int TestObject::destructor_count_ = 0;

TEST_F(TaskQueueManagerTest, PendingDelayedTasksRemovedOnShutdown) {
  Initialize(1u);

  TestObject::destructor_count_ = 0;

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())),
      delay);
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())));

  manager_.reset();

  EXPECT_EQ(2, TestObject::destructor_count_);
}

TEST_F(TaskQueueManagerTest, ManualPumping) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // However polling still works.
  EXPECT_FALSE(runners_[0]->IsQueueEmpty());

  // After pumping the task runs normally.
  runners_[0]->PumpQueue();
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingToggle) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // When pumping is enabled the task runs normally.
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_BeforePosting) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->SetQueuePriority(TaskQueue::DISABLED_PRIORITY);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->SetQueuePriority(TaskQueue::NORMAL_PRIORITY);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_AfterPosting) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->SetQueuePriority(TaskQueue::DISABLED_PRIORITY);

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->SetQueuePriority(TaskQueue::NORMAL_PRIORITY);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning_ManuallyPumpedTransitionsToAuto) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);
  runners_[0]->SetQueuePriority(TaskQueue::DISABLED_PRIORITY);
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  runners_[0]->SetQueuePriority(TaskQueue::NORMAL_PRIORITY);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithDelayedTask) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  // After pumping but before the delay period has expired, task does not run.
  runners_[0]->PumpQueue();
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_TRUE(run_order.empty());

  // Once the delay has expired, pumping causes the task to run.
  now_src_->Advance(base::TimeDelta::FromMilliseconds(5));
  runners_[0]->PumpQueue();
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithMultipleDelayedTasks) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay1(base::TimeDelta::FromMilliseconds(1));
  base::TimeDelta delay2(base::TimeDelta::FromMilliseconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay1);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay2);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               delay3);

  now_src_->Advance(base::TimeDelta::FromMilliseconds(15));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Once the delay has expired, pumping causes the task to run.
  runners_[0]->PumpQueue();
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, DelayedTasksDontAutoRunWithManualPumping) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithNonEmptyWorkQueue) {
  Initialize(1u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  // Posting two tasks and pumping twice should result in two tasks in the work
  // queue.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PumpQueue();
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PumpQueue();

  EXPECT_EQ(2u, runners_[0]->WorkQueueSizeForTest());
}

void ReentrantTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int countdown,
                       std::vector<int>* out_result) {
  out_result->push_back(countdown);
  if (--countdown) {
    runner->PostTask(FROM_HERE,
                     Bind(&ReentrantTestTask, runner, countdown, out_result));
  }
}

TEST_F(TaskQueueManagerTest, ReentrantPosting) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE,
                        Bind(&ReentrantTestTask, runners_[0], 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_.reset();
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<base::SingleThreadTaskRunner> runner,
                      std::vector<int>* run_order) {
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, run_order));
}

TEST_F(TaskQueueManagerTest, PostFromThread) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  base::Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runners_[0], &run_order));
  thread.Stop();

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

void RePostingTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int* run_count) {
  (*run_count)++;
  runner->PostTask(FROM_HERE, Bind(&RePostingTestTask,
                                   base::Unretained(runner.get()), run_count));
}

TEST_F(TaskQueueManagerTest, DoWorkCantPostItselfMultipleTimes) {
  Initialize(1u);

  int run_count = 0;
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybePostDoWorkOnMainRunner there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(1, run_count);
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 1, &run_order), true));

  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 0, &run_order));
  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&PostFromNestedRunloop, message_loop_.get(), runners_[0],
                 base::Unretained(&tasks_to_post_from_nested_loop)));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  message_loop_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 1));
}

TEST_F(TaskQueueManagerTest, WorkBatching) {
  Initialize(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  // Running one task in the host message loop should cause two posted tasks to
  // get executed.
  EXPECT_EQ(test_task_runner_->NumPendingTasks(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  // The second task runs the remaining two posted tasks.
  EXPECT_EQ(test_task_runner_->NumPendingTasks(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeup) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Shouldn't run - no other task to wake TQM.

  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Still shouldn't wake TQM.

  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a task on an auto pumped queue should wake the TQM.
  EXPECT_THAT(run_order, ElementsAre(3, 1, 2));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupWhenAlreadyAwake) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1));  // TQM was already awake.
}

TEST_F(TaskQueueManagerTest,
       AutoPumpAfterWakeupTriggeredByManuallyPumpedQueue) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);
  runners_[1]->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Shouldn't run - no other task to wake TQM.

  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  // This still shouldn't wake TQM as manual queue was not pumped.
  EXPECT_TRUE(run_order.empty());

  runners_[1]->PumpQueue();
  test_task_runner_->RunUntilIdle();
  // Executing a task on an auto pumped queue should wake the TQM.
  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

void TestPostingTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::Closure task) {
  task_runner->PostTask(FROM_HERE, task);
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupFromTask) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  // Check that a task which posts a task to an auto pump after wakeup queue
  // doesn't cause the queue to wake up.
  base::Closure after_wakeup_task = base::Bind(&TestTask, 1, &run_order);
  runners_[1]->PostTask(
      FROM_HERE, base::Bind(&TestPostingTask, runners_[0], after_wakeup_task));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Wake up the queue.
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupFromMultipleTasks) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  // Check that a task which posts a task to an auto pump after wakeup queue
  // doesn't cause the queue to wake up.
  base::Closure after_wakeup_task_1 = base::Bind(&TestTask, 1, &run_order);
  base::Closure after_wakeup_task_2 = base::Bind(&TestTask, 2, &run_order);
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestPostingTask, runners_[0],
                                              after_wakeup_task_1));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestPostingTask, runners_[0],
                                              after_wakeup_task_2));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Wake up the queue.
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 1, 2));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupBecomesQuiescent) {
  Initialize(2u);
  runners_[0]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  int run_count = 0;
  // Check that if multiple tasks reposts themselves onto a pump-after-wakeup
  // queue they don't wake each other and will eventually stop when no other
  // tasks execute.
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));
  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RePostingTestTask, runners_[0], &run_count));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  // The reposting tasks posted to the after wakeup queue shouldn't have woken
  // each other up.
  EXPECT_EQ(2, run_count);
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupWithDontWakeQueue) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue0 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 0")
          .SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP));
  scoped_refptr<internal::TaskQueueImpl> queue1 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 0")
          .SetWakeupPolicy(TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES));
  scoped_refptr<internal::TaskQueueImpl> queue2 = runners_[0];

  std::vector<int> run_order;
  queue0->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a DONT_WAKE_OTHER_QUEUES queue shouldn't wake the autopump after
  // wakeup queue.
  EXPECT_THAT(run_order, ElementsAre(2));

  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a CAN_WAKE_OTHER_QUEUES queue should wake the autopump after
  // wakeup queue.
  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

class MockTaskObserver : public base::MessageLoop::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask& task));
  MOCK_METHOD1(WillProcessTask, void(const base::PendingTask& task));
};

TEST_F(TaskQueueManagerTest, TaskObserverAdding) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(2);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(2);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, TaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);
  manager_->RemoveTaskObserver(&observer);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  message_loop_->RunUntilIdle();
}

void RemoveObserverTask(TaskQueueManager* manager,
                        base::MessageLoop::TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, TaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(3);
  manager_->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RemoveObserverTask, manager_.get(), &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverAdding) {
  InitializeWithRealMessageLoop(2u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(1);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  runners_[0]->AddTaskObserver(&observer);
  runners_[0]->RemoveTaskObserver(&observer);

  std::vector<int> run_order;
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  message_loop_->RunUntilIdle();
}

void RemoveQueueObserverTask(scoped_refptr<TaskQueue> queue,
                             base::MessageLoop::TaskObserver* observer) {
  queue->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, QueueTaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;
  runners_[0]->AddTaskObserver(&observer);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&RemoveQueueObserverTask, runners_[0], &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, ThreadCheckAfterTermination) {
  Initialize(1u);
  EXPECT_TRUE(runners_[0]->RunsTasksOnCurrentThread());
  manager_.reset();
  EXPECT_TRUE(runners_[0]->RunsTasksOnCurrentThread());
}

TEST_F(TaskQueueManagerTest, NextPendingDelayedTaskRunTime) {
  Initialize(2u);
  now_src_->Advance(base::TimeDelta::FromMicroseconds(10000));

  // With no delayed tasks.
  EXPECT_TRUE(manager_->NextPendingDelayedTaskRunTime().is_null());

  // With a non-delayed task.
  runners_[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  EXPECT_TRUE(manager_->NextPendingDelayedTaskRunTime().is_null());

  // With a delayed task.
  base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(50);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_EQ(now_src_->NowTicks() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // With another delayed task in the same queue with a longer delay.
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                               base::TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(now_src_->NowTicks() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(20);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_EQ(now_src_->NowTicks() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(10);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_EQ(now_src_->NowTicks() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // Test it updates as time progresses
  now_src_->Advance(expected_delay);
  EXPECT_EQ(now_src_->NowTicks(), manager_->NextPendingDelayedTaskRunTime());
}

TEST_F(TaskQueueManagerTest, NextPendingDelayedTaskRunTime_MultipleQueues) {
  Initialize(3u);

  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(50);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay2);
  runners_[2]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay3);

  EXPECT_EQ(now_src_->NowTicks() + delay2,
            manager_->NextPendingDelayedTaskRunTime());
}

TEST_F(TaskQueueManagerTest, DeleteTaskQueueManagerInsideATask) {
  Initialize(1u);

  runners_[0]->PostTask(
      FROM_HERE, base::Bind(&TaskQueueManagerTest::DeleteTaskQueueManager,
                            base::Unretained(this)));

  // This should not crash, assuming DoWork detects the TaskQueueManager has
  // been deleted.
  test_task_runner_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, GetAndClearSystemIsQuiescentBit) {
  Initialize(3u);

  scoped_refptr<internal::TaskQueueImpl> queue0 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 0").SetShouldMonitorQuiescence(true));
  scoped_refptr<internal::TaskQueueImpl> queue1 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 1").SetShouldMonitorQuiescence(true));
  scoped_refptr<internal::TaskQueueImpl> queue2 = manager_->NewTaskQueue(
      TaskQueue::Spec("test_queue 2").SetShouldMonitorQuiescence(false));

  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue1->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue2->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());

  queue0->PostTask(FROM_HERE, base::Bind(&NopTask));
  queue1->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  EXPECT_FALSE(manager_->GetAndClearSystemIsQuiescentBit());
  EXPECT_TRUE(manager_->GetAndClearSystemIsQuiescentBit());
}

TEST_F(TaskQueueManagerTest, IsQueueEmpty) {
  Initialize(2u);
  internal::TaskQueueImpl* queue0 = runners_[0].get();
  internal::TaskQueueImpl* queue1 = runners_[1].get();
  queue0->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  queue1->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  EXPECT_TRUE(queue0->IsQueueEmpty());
  EXPECT_TRUE(queue1->IsQueueEmpty());

  queue0->PostTask(FROM_HERE, base::Bind(NullTask));
  queue1->PostTask(FROM_HERE, base::Bind(NullTask));
  EXPECT_FALSE(queue0->IsQueueEmpty());
  EXPECT_FALSE(queue1->IsQueueEmpty());

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(queue0->IsQueueEmpty());
  EXPECT_FALSE(queue1->IsQueueEmpty());

  queue1->PumpQueue();
  EXPECT_TRUE(queue0->IsQueueEmpty());
  EXPECT_FALSE(queue1->IsQueueEmpty());

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(queue0->IsQueueEmpty());
  EXPECT_TRUE(queue1->IsQueueEmpty());
}

TEST_F(TaskQueueManagerTest, GetQueueState) {
  Initialize(2u);
  internal::TaskQueueImpl* queue0 = runners_[0].get();
  internal::TaskQueueImpl* queue1 = runners_[1].get();
  queue0->SetPumpPolicy(TaskQueue::PumpPolicy::AUTO);
  queue1->SetPumpPolicy(TaskQueue::PumpPolicy::MANUAL);

  EXPECT_EQ(TaskQueue::QueueState::EMPTY, queue0->GetQueueState());
  EXPECT_EQ(TaskQueue::QueueState::EMPTY, queue1->GetQueueState());

  queue0->PostTask(FROM_HERE, base::Bind(NullTask));
  queue0->PostTask(FROM_HERE, base::Bind(NullTask));
  queue1->PostTask(FROM_HERE, base::Bind(NullTask));
  EXPECT_EQ(TaskQueue::QueueState::NEEDS_PUMPING, queue0->GetQueueState());
  EXPECT_EQ(TaskQueue::QueueState::NEEDS_PUMPING, queue1->GetQueueState());

  test_task_runner_->SetRunTaskLimit(1);
  test_task_runner_->RunPendingTasks();
  EXPECT_EQ(TaskQueue::QueueState::HAS_WORK, queue0->GetQueueState());
  EXPECT_EQ(TaskQueue::QueueState::NEEDS_PUMPING, queue1->GetQueueState());

  test_task_runner_->ClearRunTaskLimit();
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(TaskQueue::QueueState::EMPTY, queue0->GetQueueState());
  EXPECT_EQ(TaskQueue::QueueState::NEEDS_PUMPING, queue1->GetQueueState());

  queue1->PumpQueue();
  EXPECT_EQ(TaskQueue::QueueState::EMPTY, queue0->GetQueueState());
  EXPECT_EQ(TaskQueue::QueueState::HAS_WORK, queue1->GetQueueState());

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(TaskQueue::QueueState::EMPTY, queue0->GetQueueState());
  EXPECT_EQ(TaskQueue::QueueState::EMPTY, queue1->GetQueueState());
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotSkipAHeadOfNonDelayedTask) {
  Initialize(2u);

  std::vector<int> run_order;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay);
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners_[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  now_src_->Advance(delay * 2);
  // After task 2 has run, the automatic selector will have to choose between
  // tasks 1 and 3.  The sequence numbers are used to choose between the two
  // tasks and if they are correct task 1 will run last.
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotSkipAHeadOfShorterDelayedTask) {
  Initialize(2u);

  std::vector<int> run_order;
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               delay1);
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               delay2);

  now_src_->Advance(delay1 * 2);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskWithAbsoluteRunTime) {
  Initialize(1u);

  // One task in the past, two with the exact same run time and one in the
  // future.
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks runTime1 = now_src_->NowTicks() - delay;
  base::TimeTicks runTime2 = now_src_->NowTicks();
  base::TimeTicks runTime3 = now_src_->NowTicks();
  base::TimeTicks runTime4 = now_src_->NowTicks() + delay;

  std::vector<int> run_order;
  runners_[0]->PostDelayedTaskAt(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), runTime1);
  runners_[0]->PostDelayedTaskAt(
      FROM_HERE, base::Bind(&TestTask, 2, &run_order), runTime2);
  runners_[0]->PostDelayedTaskAt(
      FROM_HERE, base::Bind(&TestTask, 3, &run_order), runTime3);
  runners_[0]->PostDelayedTaskAt(
      FROM_HERE, base::Bind(&TestTask, 4, &run_order), runTime4);

  now_src_->Advance(2 * delay);
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));
}

void CheckIsNested(bool* is_nested) {
  *is_nested = base::MessageLoop::current()->IsNested();
}

void PostAndQuitFromNestedRunloop(base::RunLoop* run_loop,
                                  base::SingleThreadTaskRunner* runner,
                                  bool* was_nested) {
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  runner->PostTask(FROM_HERE, run_loop->QuitClosure());
  runner->PostTask(FROM_HERE, base::Bind(&CheckIsNested, was_nested));
  run_loop->Run();
}

TEST_F(TaskQueueManagerTest, QuitWhileNested) {
  // This test makes sure we don't continue running a work batch after a nested
  // run loop has been exited in the middle of the batch.
  InitializeWithRealMessageLoop(1u);
  manager_->SetWorkBatchSize(2);

  bool was_nested = true;
  base::RunLoop run_loop;
  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&PostAndQuitFromNestedRunloop, base::Unretained(&run_loop),
                 runners_[0], base::Unretained(&was_nested)));

  message_loop_->RunUntilIdle();
  EXPECT_FALSE(was_nested);
}

class SequenceNumberCapturingTaskObserver
    : public base::MessageLoop::TaskObserver {
 public:
  // MessageLoop::TaskObserver overrides.
  void WillProcessTask(const base::PendingTask& pending_task) override {}
  void DidProcessTask(const base::PendingTask& pending_task) override {
    sequence_numbers_.push_back(pending_task.sequence_num);
  }

  const std::vector<int>& sequence_numbers() const { return sequence_numbers_; }

 private:
  std::vector<int> sequence_numbers_;
};

TEST_F(TaskQueueManagerTest, SequenceNumSetWhenTaskIsPosted) {
  Initialize(1u);

  SequenceNumberCapturingTaskObserver observer;
  manager_->AddTaskObserver(&observer);

  // Register four tasks that will run in reverse order.
  std::vector<int> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(30));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(4, 3, 2, 1));

  // The sequence numbers are a zero-based monotonically incrememting counter
  // which should be set when the task is posted rather than when it's enqueued
  // onto the incomming queue.
  EXPECT_THAT(observer.sequence_numbers(), ElementsAre(3, 2, 1, 0));

  manager_->RemoveTaskObserver(&observer);
}

TEST_F(TaskQueueManagerTest, NewTaskQueues) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec("foo"));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec("bar"));
  scoped_refptr<internal::TaskQueueImpl> queue3 =
      manager_->NewTaskQueue(TaskQueue::Spec("baz"));

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<int> run_order;
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue) {
  Initialize(1u);

  scoped_refptr<internal::TaskQueueImpl> queue1 =
      manager_->NewTaskQueue(TaskQueue::Spec("foo"));
  scoped_refptr<internal::TaskQueueImpl> queue2 =
      manager_->NewTaskQueue(TaskQueue::Spec("bar"));
  scoped_refptr<internal::TaskQueueImpl> queue3 =
      manager_->NewTaskQueue(TaskQueue::Spec("baz"));

  ASSERT_NE(queue1, queue2);
  ASSERT_NE(queue1, queue3);
  ASSERT_NE(queue2, queue3);

  std::vector<int> run_order;
  queue1->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  queue2->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  queue3->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  queue2->UnregisterTaskQueue();
  test_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(1, 3));
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueue_WithDelayedTasks) {
  Initialize(2u);

  // Register three delayed tasks
  std::vector<int> run_order;
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                               base::TimeDelta::FromMilliseconds(10));
  runners_[1]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                               base::TimeDelta::FromMilliseconds(20));
  runners_[0]->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                               base::TimeDelta::FromMilliseconds(30));

  runners_[1]->UnregisterTaskQueue();
  test_task_runner_->RunUntilIdle();

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(40));
  ASSERT_THAT(run_order, ElementsAre(1, 3));
}

void PostTestTasksFromNestedMessageLoop(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    scoped_refptr<base::SingleThreadTaskRunner> wake_up_runner,
    std::vector<int>* run_order) {
  base::MessageLoop::ScopedNestableTaskAllower allow(message_loop);
  main_runner->PostNonNestableTask(FROM_HERE,
                                   base::Bind(&TestTask, 1, run_order));
  // The following should never get executed.
  wake_up_runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, run_order));
  message_loop->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, DeferredNonNestableTaskDoesNotTriggerWakeUp) {
  // This test checks that running (i.e., deferring) a non-nestable task in a
  // nested run loop does not trigger the pumping of an on-wakeup queue.
  InitializeWithRealMessageLoop(2u);
  runners_[1]->SetPumpPolicy(TaskQueue::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&PostTestTasksFromNestedMessageLoop, message_loop_.get(),
                 runners_[0], runners_[1], base::Unretained(&run_order)));

  message_loop_->RunUntilIdle();
  ASSERT_THAT(run_order, ElementsAre(1));
}

namespace {

class MockObserver : public TaskQueueManager::Observer {
 public:
  MOCK_METHOD1(OnUnregisterTaskQueue,
               void(const scoped_refptr<internal::TaskQueueImpl>& queue));
};

}  // namespace

TEST_F(TaskQueueManagerTest, OnUnregisterTaskQueue) {
  Initialize(0u);

  MockObserver observer;
  manager_->SetObserver(&observer);

  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec("test_queue"));

  EXPECT_CALL(observer, OnUnregisterTaskQueue(_)).Times(1);
  task_queue->UnregisterTaskQueue();

  manager_->SetObserver(nullptr);
}

TEST_F(TaskQueueManagerTest, ScheduleDelayedWorkIsNotReEntrant) {
  Initialize(1u);

  // Post two tasks into the past. The second one used to trigger a deadlock
  // because it tried to re-entrantly wake the first task in the same queue.
  runners_[0]->PostDelayedTaskAt(
      FROM_HERE, base::Bind(&NullTask),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(100));
  runners_[0]->PostDelayedTaskAt(
      FROM_HERE, base::Bind(&NullTask),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(200));
  test_task_runner_->RunUntilIdle();
}

void HasOneRefTask(std::vector<bool>* log, internal::TaskQueueImpl* tq) {
  log->push_back(tq->HasOneRef());
}

TEST_F(TaskQueueManagerTest, UnregisterTaskQueueInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  // We retain a reference to the task queue even when the manager has deleted
  // its reference.
  scoped_refptr<internal::TaskQueueImpl> task_queue =
      manager_->NewTaskQueue(TaskQueue::Spec("test_queue"));

  std::vector<bool> log;
  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;

  // Inside a nested run loop, call task_queue->UnregisterTaskQueue, bookended
  // by calls to HasOneRefTask to make sure the manager doesn't release its
  // reference until the nested run loop exits.
  // NB: This first HasOneRefTask is a sanity check.
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&HasOneRefTask, base::Unretained(&log),
                                base::Unretained(task_queue.get())),
                     true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&internal::TaskQueueImpl::UnregisterTaskQueue,
                 base::Unretained(task_queue.get())), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&HasOneRefTask, base::Unretained(&log),
                                base::Unretained(task_queue.get())),
                     true));
  runners_[0]->PostTask(
      FROM_HERE,
      base::Bind(&PostFromNestedRunloop, message_loop_.get(), runners_[0],
                 base::Unretained(&tasks_to_post_from_nested_loop)));
  message_loop_->RunUntilIdle();

  // Add a final call to HasOneRefTask.  This gives the manager a chance to
  // release its reference, and checks that it has.
  runners_[0]->PostTask(FROM_HERE,
                        base::Bind(&HasOneRefTask, base::Unretained(&log),
                                   base::Unretained(task_queue.get())));
  message_loop_->RunUntilIdle();

  EXPECT_THAT(log, ElementsAre(false, false, true));
}

}  // namespace scheduler
