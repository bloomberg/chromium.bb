// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "content/renderer/scheduler/task_queue_selector.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    manager_ = make_scoped_ptr(
        new TaskQueueManager(num_queues, test_task_runner_, selector_.get()));
  }

  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
  scoped_ptr<SelectorForTest> selector_;
  scoped_ptr<TaskQueueManager> manager_;
};

void TestTask(int value, std::vector<int>* out_result) {
  out_result->push_back(value);
}

TEST_F(TaskQueueManagerTest, SingleQueuePosting) {
  Initialize(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

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
  EXPECT_EQ(1, run_order[0]);
  EXPECT_EQ(2, run_order[1]);
  EXPECT_EQ(3, run_order[2]);
}

TEST_F(TaskQueueManagerTest, MultiQueuePosting) {
  Initialize(3u);
  EXPECT_EQ(3u, selector_->work_queues().size());

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
  EXPECT_EQ(1, run_order[0]);
  EXPECT_EQ(3, run_order[1]);
  EXPECT_EQ(5, run_order[2]);
  EXPECT_EQ(2, run_order[3]);
  EXPECT_EQ(4, run_order[4]);
  EXPECT_EQ(6, run_order[5]);
}

TEST_F(TaskQueueManagerTest, NonNestableTaskPosting) {
  Initialize(1u);
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostNonNestableTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));

  // Non-nestable tasks never make it to the selector.
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_order[0]);
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
  EXPECT_EQ(1, run_order[0]);
}

TEST_F(TaskQueueManagerTest, ManualPumping) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

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
  EXPECT_EQ(1, run_order[0]);
}

TEST_F(TaskQueueManagerTest, ManualPumpingToggle) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  // Posting a task when pumping is disabled doesn't result in work getting
  // posted.
  runner->PostTask(FROM_HERE, base::Bind(&TestTask, 1, &run_order));
  EXPECT_FALSE(test_task_runner_->HasPendingTask());

  // When pumping is enabled the task runs normally.
  manager_->SetAutoPump(0, true);
  EXPECT_TRUE(test_task_runner_->HasPendingTask());
  selector_->AppendQueueToService(0);
  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(1, run_order[0]);
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
  EXPECT_EQ(1, run_order[0]);
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithDelayedTask) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

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
  EXPECT_EQ(1, run_order[0]);
}

TEST_F(TaskQueueManagerTest, ManualPumpingWithNonEmptyWorkQueue) {
  Initialize(1u);
  manager_->SetAutoPump(0, false);

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
  EXPECT_EQ(1u, selector_->work_queues().size());

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  runner->PostTask(FROM_HERE, Bind(&ReentrantTestTask, runner, 3, &run_order));

  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);
  selector_->AppendQueueToService(0);

  test_task_runner_->RunUntilIdle();
  EXPECT_EQ(3, run_order[0]);
  EXPECT_EQ(2, run_order[1]);
  EXPECT_EQ(1, run_order[2]);
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
  base::MessageLoop message_loop;
  selector_ = make_scoped_ptr(new SelectorForTest);
  manager_ = make_scoped_ptr(
      new TaskQueueManager(1u, message_loop.task_runner(), selector_.get()));

  std::vector<int> run_order;
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      manager_->TaskRunnerForQueue(0);

  base::Thread thread("TestThread");
  thread.Start();
  thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&PostTaskToRunner, runner, &run_order));
  thread.Stop();

  selector_->AppendQueueToService(0);
  message_loop.RunUntilIdle();
  EXPECT_EQ(1, run_order[0]);
}

}  // namespace
}  // namespace content
