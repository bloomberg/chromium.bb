// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
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
  SelectorForTest() {}

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

  void AppendQueueToService(size_t queue_index) {
    queues_to_service_.push_back(queue_index);
  }

  const std::vector<const base::TaskQueue*>& work_queues() {
    return work_queues_;
  }

  void AsValueInto(base::trace_event::TracedValue* state) const override {
  }

 private:
  std::deque<size_t> queues_to_service_;
  std::vector<const base::TaskQueue*> work_queues_;

  DISALLOW_COPY_AND_ASSIGN(SelectorForTest);
};

class TaskQueueManagerTest : public testing::Test {
 protected:
  void Initialize(size_t num_queues) {
    test_task_runner_ = make_scoped_refptr(new base::TestSimpleTaskRunner());
    selector_ = make_scoped_ptr(new SelectorForTest);
    manager_ = make_scoped_ptr(new TaskQueueManager(
        num_queues, NestableTaskRunnerForTest::Create(test_task_runner_.get()),
        selector_.get()));
    EXPECT_EQ(num_queues, selector_->work_queues().size());
  }

  void InitializeWithRealMessageLoop(size_t num_queues) {
    message_loop_.reset(new base::MessageLoop());
    selector_ = make_scoped_ptr(new SelectorForTest);
    manager_ = make_scoped_ptr(new TaskQueueManager(
        num_queues,
        RendererSchedulerMessageLoopDelegate::Create(message_loop_.get()),
        selector_.get()));
    EXPECT_EQ(num_queues, selector_->work_queues().size());
  }

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
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
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3));
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);

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

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);

  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskExecutesInExpectedOrder) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));
  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 5, &run_order));

  message_loop_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(TaskQueueManagerTest, NonNestableTaskDoesntExecuteInNestedLoop) {
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

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
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  EXPECT_TRUE(manager_->IsQueueEmpty(0));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(manager_->IsQueueEmpty(0));

  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(manager_->IsQueueEmpty(0));
}

TEST_F(TaskQueueManagerTest, DelayedTaskPosting) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), delay);
  EXPECT_EQ(delay, test_task_runner_->NextPendingTaskDelay());
  EXPECT_TRUE(manager_->IsQueueEmpty(0));
  EXPECT_TRUE(run_order.empty());

  // The task is inserted to the incoming queue only after the delay.
  test_task_runner_->RunPendingTasks();
  EXPECT_FALSE(manager_->IsQueueEmpty(0));
  EXPECT_TRUE(run_order.empty());

  // After the delay the task runs normally.
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DelayedTaskDoesNotStayDelayed) {
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay);
  test_task_runner_->RunPendingTasks();

  // Reload the work queue so we see the next pending task. It should no longer
  // be marked as delayed.
  manager_->PumpQueue(0);
  EXPECT_TRUE(selector_->work_queues()[0]->front().delayed_run_time.is_null());

  // Let the task run normally.
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumping) {
  Initialize(1u);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // However polling still works.
  EXPECT_FALSE(manager_->IsQueueEmpty(0));

  // After pumping the task runs normally.
  manager_->PumpQueue(0);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingToggle) {
  Initialize(1u);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // When pumping is enabled the task runs normally.
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::AUTO);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, DenyRunning) {
  Initialize(1u);

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
  Initialize(1u);
  manager_->SetPumpPolicy(0, TaskQueueManager::PumpPolicy::MANUAL);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a delayed task when pumping will apply the delay, but won't cause
  // work to executed afterwards.
  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  runner->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, 1, &run_order), delay);
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // After pumping the task runs normally.
  manager_->PumpQueue(0);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(1));
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithNonEmptyWorkQueue) {
  Initialize(1u);
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
  Initialize(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, Bind(&ReentrantTestTask, runner, 3, &run_order));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(3, 2, 1));
}

TEST_F(TaskQueueManagerTest, NoTasksAfterShutdown) {
  Initialize(1u);

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
  InitializeWithRealMessageLoop(1u);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::Thread thread("TestThread");
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runner, &run_order));
  thread.Stop();

  selector_->AppendQueueToService(0);
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
  Initialize(1u);
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  int run_count = 0;
  runner->PostTask(FROM_HERE,
                   base::Bind(&RePostingTestTask, runner, &run_count));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunPendingTasks();
  // NOTE without the executing_task_ check in MaybePostDoWorkOnMainRunner there
  // will be two tasks here.
  EXPECT_EQ(1u, test_task_runner_->GetPendingTasks().size());
  EXPECT_EQ(1, run_count);
}

TEST_F(TaskQueueManagerTest, PostFromNestedRunloop) {
  InitializeWithRealMessageLoop(1u);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

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
  Initialize(1u);

  manager_->SetWorkBatchSize(2);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 3, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 4, &run_order));

  // Running one task in the host message loop should cause two posted tasks to
  // get executed.
  EXPECT_EQ(test_task_runner_->GetPendingTasks().size(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2));

  // The second task runs the remaining two posted tasks.
  EXPECT_EQ(test_task_runner_->GetPendingTasks().size(), 1u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(1, 2, 3, 4));
}

void AdvanceNowTestTask(int value,
                        std::vector<int>* out_result,
                        scoped_refptr<cc::TestNowSource> time_source,
                        base::TimeDelta delta) {
  TestTask(value, out_result);
  time_source->AdvanceNow(delta);
}

TEST_F(TaskQueueManagerTest, InterruptWorkBatchForDelayedTask) {
  scoped_refptr<cc::TestNowSource> clock(cc::TestNowSource::Create());
  Initialize(1u);

  manager_->SetWorkBatchSize(2);
  manager_->SetTimeSourceForTesting(clock);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  base::TimeDelta delta(base::TimeDelta::FromMilliseconds(10));
  runner->PostTask(
      FROM_HERE, base::Bind(&AdvanceNowTestTask, 2, &run_order, clock, delta));
  runner->PostTask(
      FROM_HERE, base::Bind(&AdvanceNowTestTask, 3, &run_order, clock, delta));

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(5));
  runner->PostDelayedTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order),
                          delay);

  // At this point we have two posted tasks: one for DoWork and one of the
  // delayed task. Only the first non-delayed task should get executed because
  // the work batch is interrupted by the pending delayed task.
  EXPECT_EQ(test_task_runner_->GetPendingTasks().size(), 2u);
  test_task_runner_->RunPendingTasks();
  EXPECT_THAT(run_order, ElementsAre(2));

  // Running all remaining tasks should execute both pending tasks.
  test_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_order, ElementsAre(2, 3, 1));
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeup) {
  Initialize(2u);
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
  Initialize(2u);
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
  Initialize(2u);
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
  Initialize(2u);
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
  Initialize(2u);
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

void NullTestTask() {
}

TEST_F(TaskQueueManagerTest, AutoPumpAfterWakeupBecomesQuiescent) {
  Initialize(2u);
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
  runners[1]->PostTask(FROM_HERE, base::Bind(&NullTestTask));
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
  InitializeWithRealMessageLoop(1u);
  MockTaskObserver observer;

  manager_->SetWorkBatchSize(2);
  manager_->AddTaskObserver(&observer);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 2, &run_order));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  // Two pairs of callbacks for the tasks above plus another one for the
  // DoWork() posted by the task queue manager.
  EXPECT_CALL(observer, WillProcessTask(_)).Times(3);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(3);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, TaskObserverRemoving) {
  InitializeWithRealMessageLoop(1u);
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

  selector_->AppendQueueToService(0);
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

  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);
  runner->PostTask(FROM_HERE,
                   base::Bind(&RemoveObserverTask, manager_.get(), &observer));

  selector_->AppendQueueToService(0);

  EXPECT_CALL(observer, WillProcessTask(_)).Times(1);
  EXPECT_CALL(observer, DidProcessTask(_)).Times(0);
  message_loop_->RunUntilIdle();
}

TEST_F(TaskQueueManagerTest, ThreadCheckAfterTermination) {
  Initialize(1);
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);
  EXPECT_TRUE(runner->RunsTasksOnCurrentThread());
  manager_.reset();
  EXPECT_TRUE(runner->RunsTasksOnCurrentThread());
}

}  // namespace
}  // namespace content

