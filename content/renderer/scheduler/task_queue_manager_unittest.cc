// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include "base/threading/thread.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/test_now_source.h"
#include "content/renderer/scheduler/nestable_task_runner_for_test.h"
#include "content/renderer/scheduler/renderer_scheduler_message_loop_delegate.h"
#include "content/renderer/scheduler/task_queue_selector.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;
using testing::_;

namespace content {
namespace {

class SelectorForTest : public TaskQueueSelector {
 public:
  ~SelectorForTest() override {}

  virtual void AppendQueueToService(size_t queue_index) = 0;

  virtual const std::vector<const base::TaskQueue*>& work_queues() = 0;

  void AsValueInto(base::trace_event::TracedValue* state) const override {}
};

// Always selects queue 0.
class AutomaticSelectorForTest : public SelectorForTest {
 public:
  AutomaticSelectorForTest() {}
  ~AutomaticSelectorForTest() override {}

  void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) override {
    work_queues_ = work_queues;
  }

  bool SelectWorkQueueToService(size_t* out_queue_index) override {
    for (size_t i = 0; i < work_queues_.size(); i++) {
      if (!work_queues_[i]->empty()) {
        *out_queue_index = i;
        return true;
      }
    }
    return false;
  }

  void AppendQueueToService(size_t queue_index) override {
    DCHECK(false) << "Not supported";
  }

  const std::vector<const base::TaskQueue*>& work_queues() override {
    return work_queues_;
  }

 private:
  std::vector<const base::TaskQueue*> work_queues_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticSelectorForTest);
};

class ExplicitSelectorForTest : public SelectorForTest {
 public:
  ExplicitSelectorForTest() {}
  ~ExplicitSelectorForTest() override {}

  void RegisterWorkQueues(
      const std::vector<const base::TaskQueue*>& work_queues) override {
    work_queues_ = work_queues;
  }

  bool SelectWorkQueueToService(size_t* out_queue_index) override {
    if (queues_to_service_.empty())
      return false;
    *out_queue_index = queues_to_service_.front();
    queues_to_service_.pop_front();
    return true;
  }

  void AppendQueueToService(size_t queue_index) override {
    queues_to_service_.push_back(queue_index);
  }

  const std::vector<const base::TaskQueue*>& work_queues() override {
    return work_queues_;
  }

 private:
  std::deque<size_t> queues_to_service_;
  std::vector<const base::TaskQueue*> work_queues_;

  DISALLOW_COPY_AND_ASSIGN(ExplicitSelectorForTest);
};

class TaskQueueManagerTest : public testing::Test {
 protected:
  enum class SelectorType {
    Automatic,
    Explicit,
  };

  void Initialize(size_t num_queues, SelectorType type) {
    now_src_ = cc::TestNowSource::Create(1000);
    test_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(now_src_, false));
    selector_ = make_scoped_ptr(createSelectorForTest(type));
    manager_ = make_scoped_ptr(new TaskQueueManager(
        num_queues, NestableTaskRunnerForTest::Create(test_task_runner_.get()),
        selector_.get()));
    manager_->SetTimeSourceForTesting(now_src_);

    EXPECT_EQ(num_queues, selector_->work_queues().size());
  }

  void InitializeWithRealMessageLoop(size_t num_queues, SelectorType type) {
    message_loop_.reset(new base::MessageLoop());
    selector_ = make_scoped_ptr(createSelectorForTest(type));
    manager_ = make_scoped_ptr(new TaskQueueManager(
        num_queues,
        RendererSchedulerMessageLoopDelegate::Create(message_loop_.get()),
        selector_.get()));
    EXPECT_EQ(num_queues, selector_->work_queues().size());
  }

  SelectorForTest* createSelectorForTest(SelectorType type) {
    switch (type) {
      case SelectorType::Automatic:
        return new AutomaticSelectorForTest();

      case SelectorType::Explicit:
        return new ExplicitSelectorForTest();
    }

    return nullptr;
  }

  scoped_refptr<cc::TestNowSource> now_src_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> test_task_runner_;
  scoped_ptr<SelectorForTest> selector_;
  scoped_ptr<TaskQueueManager> manager_;
  scoped_ptr<base::MessageLoop> message_loop_;
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

void TestTask(int value, std::vector<int>* out_result) {
  out_result->push_back(value);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u, SelectorType::Explicit);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[3] = {
      manager_->TaskRunnerForQueue(0),
      manager_->TaskRunnerForQueue(1),
      manager_->TaskRunnerForQueue(2)};

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(2);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(2);

  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runners[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));
  runners[2]->PostTask(FROM_HERE, base::Bind(&TestTask, 6, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 3, 5, 2, 4, 6));
}

void NopTask() {
}

TEST_F(TaskQueueManagerTest, NowNotCalledWhenThereAreNoDelayedTasks) {
  Initialize(3u, SelectorType::Explicit);

  scoped_refptr<cc::TestNowSource> now_src = cc::TestNowSource::Create(1000);
  manager_->SetTimeSourceForTesting(now_src);

  scoped_refptr<base::SingleThreadTaskRunner> runners[3] = {
      manager_->TaskRunnerForQueue(0),
      manager_->TaskRunnerForQueue(1),
      manager_->TaskRunnerForQueue(2)};

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(2);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(2);

  runners[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners[2]->PostTask(FROM_HERE, base::Bind(&NopTask));
  runners[2]->PostTask(FROM_HERE, base::Bind(&NopTask));

  test_task_runner_->RunUntilIdle();

  EXPECT_EQ(0, now_src->NumNowCalls());
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 3, &run_order), false));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 4, &run_order), true));
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 5, &run_order), true));

  runner->PostTask(
      FROM_HERE,
      base::Bind(&PostFromNestedRunloop, message_loop_.get(), runner,
                 base::Unretained(&tasks_to_post_from_nested_loop)));

  message_loop_->RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(run_order, ElementsAre(1, 2, 4, 5, 3));
}

TEST_F(TaskQueueManagerTest, QueuePolling) {
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  EXPECT_TRUE(manager_->IsQueueEmpty(0));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(manager_->IsQueueEmpty(0));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->IsQueueEmpty(0));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), delay);
  EXPECT_EQ(delay, test_task_runner_->DelayToNextTaskTime());
  EXPECT_TRUE(manager_->IsQueueEmpty(0));
  EXPECT_TRUE(run_order.empty());

  // The task doesn't run before the delay has completed.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(9));
  EXPECT_TRUE(run_order.empty());

  // After the delay has completed, the task runs normally.
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(1));
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting_MultipleTasks_DecendingOrder) {
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          base::TimeDelta::FromMilliseconds(10));

  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                          base::TimeDelta::FromMilliseconds(8));

  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
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
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          base::TimeDelta::FromMilliseconds(1));

  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                          base::TimeDelta::FromMilliseconds(5));

  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
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
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay);
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                          delay);
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
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
  Initialize(1u, SelectorType::Automatic);

  TestObject::destructor_count_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestObject::Run, base::Owned(new TestObject())),
      delay);

  manager_.reset();

  EXPECT_EQ(1, TestObject::destructor_count_);
}

TEST_F(TaskQueueManagerTest, ManualPumping) {
  Initialize(1u, SelectorType::Automatic);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // However polling still works.
  EXPECT_FALSE(manager_->IsQueueEmpty(0));

  // After pumping the task runs normally.
  manager_->PumpQueue(0);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingToggle) {
  Initialize(1u, SelectorType::Automatic);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTasks());

  // When pumping is enabled the task runs normally.
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AUTO);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning) {
  Initialize(1u, SelectorType::Explicit);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  // Since we haven't appended a work queue to be selected, the task doesn't
  // run.
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Pumping the queue again with a selected work queue runs the task.
  manager_->PumpQueue(0);
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithDelayedTask) {
  Initialize(1u, SelectorType::Automatic);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), delay);

  // After pumping but before the delay period has expired, task does not run.
  manager_->PumpQueue(0);
  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(5));
  EXPECT_TRUE(run_order.empty());

  // Once the delay has expired, pumping causes the task to run.
  now_src_->AdvanceNow(base::TimeDelta::FromMilliseconds(5));
  manager_->PumpQueue(0);
  EXPECT_TRUE(test_task_runner_->HasPendingTasks());
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithMultipleDelayedTasks) {
  Initialize(1u, SelectorType::Automatic);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay1(base::TimeDelta::FromMilliseconds(1));
  base::TimeDelta delay2(base::TimeDelta::FromMilliseconds(10));
  base::TimeDelta delay3(base::TimeDelta::FromMilliseconds(20));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay1);
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order),
                          delay2);
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order),
                          delay3);

  now_src_->AdvanceNow(base::TimeDelta::FromMilliseconds(15));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Once the delay has expired, pumping causes the task to run.
  manager_->PumpQueue(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2));
}

TEST_F(TaskQueueManagerTest, DelayedTasksDontAutoRunWithManualPumping) {
  Initialize(1u, SelectorType::Automatic);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay);

  test_task_runner_->RunForPeriod(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(run_order.empty());
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithNonEmptyWorkQueue) {
  Initialize(1u, SelectorType::Automatic);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting two tasks and pumping twice should result in two tasks in the work
  // queue.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_->PumpQueue(0);
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  manager_->PumpQueue(0);

  EXPECT_EQ(2u, selector_->work_queues()[0]->size());
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
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, Bind(&ReentrantTestTask, runner, 3, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  manager_.reset();
  selector_.reset();
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

void PostTaskToRunner(scoped_refptr<base::SingleThreadTaskRunner> runner,
                      std::vector<int>* run_order) {
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, run_order));
}

TEST_F(TaskQueueManagerTest, PostFromThread) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::Thread thread("TestThread");
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runner, &run_order));
  thread.Stop();

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

void RePostingTestTask(scoped_refptr<base::SingleThreadTaskRunner> runner,
                       int* run_count) {
  (*run_count)++;
  runner->PostTask(
      FROM_HERE,
      Bind(&RePostingTestTask, base::Unretained(runner.get()), run_count));
}

TEST_F(TaskQueueManagerTest, DoWorkCantPostItselfMultipleTimes) {
  Initialize(1u, SelectorType::Automatic);
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  int run_count = 0;
  runner->PostTask(FROM_HERE,
                   base::Bind(&RePostingTestTask, runner, &run_count));

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybePostDoWorkOnMainRunner there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->NumPendingTasks());
  EXPECT_EQ(1, run_count);
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  std::vector<std::pair<base::Closure, bool>> tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(
      std::make_pair(base::Bind(&TestTask, 1, &run_order), true));

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 0, &run_order));
  runner->PostTask(
      FROM_HERE, base::Bind(&PostFromNestedRunloop, message_loop_.get(), runner,
                            base::Unretained(&tasks_to_post_from_nested_loop)));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  message_loop_->RunUntilIdle();

  EXPECT_THAT(run_order, ElementsAre(0, 2, 1));
}

TEST_F(TaskQueueManagerTest, WorkBatching) {
  Initialize(1u, SelectorType::Automatic);

  manager_->SetWorkBatchSize(2);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

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
  Initialize(2u, SelectorType::Explicit);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Shouldn't run - no other task to wake TQM.

  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Still shouldn't wake TQM.

  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  // Executing a task on an auto pumped queue should wake the TQM.
  EXPECT_THAT(run_order, ElementsAre(3, 1, 2));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupWhenAlreadyAwake) {
  Initialize(2u, SelectorType::Explicit);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(0);

  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1));  // TQM was already awake.
}

TEST_F(TaskQueueManagerTest,
       AutoPumpAfterWakeupTriggeredByManuallyPumpedQueue) {
  Initialize(2u, SelectorType::Explicit);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AFTER_WAKEUP);
  manager_->SetPumpPolicy(1, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(0);

  runners[0]->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());  // Shouldn't run - no other task to wake TQM.

  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  // This still shouldn't wake TQM as manual queue was not pumped.
  EXPECT_TRUE(run_order.empty());

  manager_->PumpQueue(1);
  test_task_runner_->RunUntilIdle();
  // Executing a task on an auto pumped queue should wake the TQM.
  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

void TestPostingTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::Closure task) {
  task_runner->PostTask(FROM_HERE, task);
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupFromTask) {
  Initialize(2u, SelectorType::Explicit);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(0);

  // Check that a task which posts a task to an auto pump after wakeup queue
  // doesn't cause the queue to wake up.
  base::Closure after_wakeup_task = base::Bind(&TestTask, 1, &run_order);
  runners[1]->PostTask(
      FROM_HERE,
      base::Bind(&TestPostingTask, runners[0], after_wakeup_task));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Wake up the queue.
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 1));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupFromMultipleTasks) {
  Initialize(2u, SelectorType::Explicit);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AFTER_WAKEUP);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  // Check that a task which posts a task to an auto pump after wakeup queue
  // doesn't cause the queue to wake up.
  base::Closure after_wakeup_task_1 = base::Bind(&TestTask, 1, &run_order);
  base::Closure after_wakeup_task_2 = base::Bind(&TestTask, 2, &run_order);
  runners[1]->PostTask(
      FROM_HERE,
      base::Bind(&TestPostingTask, runners[0], after_wakeup_task_1));
  runners[1]->PostTask(
      FROM_HERE,
      base::Bind(&TestPostingTask, runners[0], after_wakeup_task_2));
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Wake up the queue.
  runners[1]->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 1, 2));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupBecomesQuiescent) {
  Initialize(2u, SelectorType::Explicit);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AFTER_WAKEUP);

  int run_count = 0;
  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  selector_->AppendQueueToService(1);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  // Append extra service queue '0' entries to the selector otherwise test will
  // finish even if the RePostingTestTask woke each other up.
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  // Check that if multiple tasks reposts themselves onto a pump-after-wakeup
  // queue they don't wake each other and will eventually stop when no other
  // tasks execute.
  runners[0]->PostTask(FROM_HERE,
                       base::Bind(&RePostingTestTask, runners[0], &run_count));
  runners[0]->PostTask(FROM_HERE,
                       base::Bind(&RePostingTestTask, runners[0], &run_count));
  runners[1]->PostTask(FROM_HERE, base::Bind(&NopTask));
  test_task_runner_->RunUntilIdle();
  // The reposting tasks posted to the after wakeup queue shouldn't have woken
  // each other up.
  EXPECT_EQ(2, run_count);
}

class MockTaskObserver : public base::MessageLoop::TaskObserver {
 public:
  MOCK_METHOD1(DidProcessTask, void(const base::PendingTask& task));
  MOCK_METHOD1(WillProcessTask, void(const base::PendingTask& task));
};

TEST_F(TaskQueueManagerTest, TaskObserverAdding) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  // Two pairs of callbacks for the tasks above plus another one for the
  // DoWork() posted by the task queue manager.
  EXPECT_CALL(observer, WillProcessTask(_)).Times(3);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(3);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, TaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);
  manager_->RemoveTaskObserver(&observer);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(0);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);

  message_loop_->RunUntilIdle();
}

void RemoveObserverTask(TaskQueueManager* manager,
                        base::MessageLoop::TaskObserver* observer) {
  manager->RemoveTaskObserver(observer);
}

TEST_F(TaskQueueManagerTest, TaskObserverRemovingInsideTask) {
  InitializeWithRealMessageLoop(1u, SelectorType::Automatic);
  MockTaskObserver observer;
  manager_->SetWorkBatchSize(3);
  manager_->AddTaskObserver(&observer);

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);
  runner->PostTask(FROM_HERE,
                   base::Bind(&RemoveObserverTask, manager_.get(), &observer));

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, ThreadCheckAfterTermination) {
  Initialize(1u, SelectorType::Automatic);
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);
  EXPECT_TRUE(runner->RunsTasksOnCurrentThread());
  manager_.reset();
  EXPECT_TRUE(runner->RunsTasksOnCurrentThread());
}

TEST_F(TaskQueueManagerTest, NextPendingDelayedTaskRunTime) {
  scoped_refptr<cc::TestNowSource> clock(cc::TestNowSource::Create());
  Initialize(2u, SelectorType::Explicit);
  manager_->SetTimeSourceForTesting(clock);

  scoped_refptr<base::SingleThreadTaskRunner> runners[2] = {
      manager_->TaskRunnerForQueue(0), manager_->TaskRunnerForQueue(1)};

  // With no delayed tasks.
  EXPECT_TRUE(manager_->NextPendingDelayedTaskRunTime().is_null());

  // With a non-delayed task.
  runners[0]->PostTask(FROM_HERE, base::Bind(&NopTask));
  EXPECT_TRUE(manager_->NextPendingDelayedTaskRunTime().is_null());

  // With a delayed task.
  base::TimeDelta expected_delay = base::TimeDelta::FromMilliseconds(50);
  runners[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_EQ(clock->Now() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // With another delayed task in the same queue with a longer delay.
  runners[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask),
                              base::TimeDelta::FromMilliseconds(100));
  EXPECT_EQ(clock->Now() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // With another delayed task in the same queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(20);
  runners[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_EQ(clock->Now() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // With another delayed task in a different queue with a shorter delay.
  expected_delay = base::TimeDelta::FromMilliseconds(10);
  runners[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), expected_delay);
  EXPECT_EQ(clock->Now() + expected_delay,
            manager_->NextPendingDelayedTaskRunTime());

  // Test it updates as time progresses
  clock->AdvanceNow(expected_delay);
  EXPECT_EQ(clock->Now(), manager_->NextPendingDelayedTaskRunTime());
}

TEST_F(TaskQueueManagerTest, NextPendingDelayedTaskRunTime_MultipleQueues) {
  Initialize(3u, SelectorType::Automatic);

  scoped_refptr<base::SingleThreadTaskRunner> runners[3] = {
      manager_->TaskRunnerForQueue(0),
      manager_->TaskRunnerForQueue(1),
      manager_->TaskRunnerForQueue(2)};

  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(50);
  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta delay3 = base::TimeDelta::FromMilliseconds(10);
  runners[0]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay1);
  runners[1]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay2);
  runners[2]->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay3);

  EXPECT_EQ(now_src_->Now() + delay2,
            manager_->NextPendingDelayedTaskRunTime());
}

}  // namespace
}  // namespace content

