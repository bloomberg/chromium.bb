// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/prioritizing_task_queue_selector.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/pending_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

class MockObserver : public TaskQueueSelector::Observer {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}

  MOCK_METHOD0(OnTaskQueueEnabled, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class PrioritizingTaskQueueSelectorForTest
    : public PrioritizingTaskQueueSelector {
 public:
  PrioritizingTaskQueueSelectorForTest() {}
  ~PrioritizingTaskQueueSelectorForTest() override {}

  using PrioritizingTaskQueueSelector::DisableQueueInternal;
};

class PrioritizingTaskQueueSelectorTest : public testing::Test {
 public:
  PrioritizingTaskQueueSelectorTest()
      : test_closure_(
            base::Bind(&PrioritizingTaskQueueSelectorTest::TestFunction)) {}
  ~PrioritizingTaskQueueSelectorTest() override {}

  std::vector<base::PendingTask> GetTasks(int count) {
    std::vector<base::PendingTask> tasks;
    for (int i = 0; i < count; i++) {
      base::PendingTask task = base::PendingTask(FROM_HERE, test_closure_);
      task.sequence_num = i;
      tasks.push_back(task);
    }
    return tasks;
  }

  void PushTasks(const std::vector<base::PendingTask>& tasks,
                 const size_t queue_indices[]) {
    for (size_t i = 0; i < tasks.size(); i++) {
      task_queues_[queue_indices[i]]->push(tasks[i]);
    }
  }

  std::vector<size_t> PopTasks() {
    std::vector<size_t> order;
    size_t chosen_queue_index;
    while (selector_.SelectWorkQueueToService(&chosen_queue_index)) {
      order.push_back(chosen_queue_index);
      task_queues_[chosen_queue_index]->pop();
    }
    return order;
  }

  static void TestFunction() {}

 protected:
  void SetUp() final {
    std::vector<const base::TaskQueue*> const_task_queues;
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      scoped_ptr<base::TaskQueue> task_queue(new base::TaskQueue());
      const_task_queues.push_back(task_queue.get());
      task_queues_.push_back(task_queue.release());
    }
    selector_.RegisterWorkQueues(const_task_queues);
    for (size_t i = 0; i < kTaskQueueCount; i++)
      EXPECT_TRUE(selector_.IsQueueEnabled(i)) << i;
  }

  const size_t kTaskQueueCount = 5;
  base::Closure test_closure_;
  PrioritizingTaskQueueSelectorForTest selector_;
  ScopedVector<base::TaskQueue> task_queues_;
};

TEST_F(PrioritizingTaskQueueSelectorTest, TestDefaultPriority) {
  std::vector<base::PendingTask> tasks = GetTasks(5);
  size_t queue_order[] = {4, 3, 2, 1, 0};
  PushTasks(tasks, queue_order);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4, 3, 2, 1, 0));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestHighPriority) {
  std::vector<base::PendingTask> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(2, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestBestEffortPriority) {
  std::vector<base::PendingTask> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(
      0, PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  selector_.SetQueuePriority(2, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2, 1, 3, 4, 0));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestControlPriority) {
  std::vector<base::PendingTask> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(4,
                             PrioritizingTaskQueueSelector::CONTROL_PRIORITY);
  EXPECT_TRUE(selector_.IsQueueEnabled(4));
  selector_.SetQueuePriority(2, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
  EXPECT_TRUE(selector_.IsQueueEnabled(2));
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4, 2, 0, 1, 3));
}

TEST_F(PrioritizingTaskQueueSelectorTest,
       TestDisableReturnsTrueIfQueueWasEnabled) {
  selector_.EnableQueue(1, PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
  EXPECT_TRUE(selector_.DisableQueueInternal(1));
}

TEST_F(PrioritizingTaskQueueSelectorTest,
       TestDisableReturnsFalseIfQueueWasAlreadyDisabled) {
  selector_.DisableQueue(1);
  EXPECT_FALSE(selector_.DisableQueueInternal(1));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestDisableEnable) {
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);

  std::vector<base::PendingTask> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.DisableQueue(2);
  EXPECT_FALSE(selector_.IsQueueEnabled(2));
  selector_.DisableQueue(4);
  EXPECT_FALSE(selector_.IsQueueEnabled(4));
  EXPECT_THAT(PopTasks(), testing::ElementsAre(0, 1, 3));

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled()).Times(2);
  selector_.EnableQueue(2, PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2));
  selector_.EnableQueue(4, PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestEmptyQueues) {
  size_t chosen_queue_index = 0;
  EXPECT_FALSE(selector_.SelectWorkQueueToService(&chosen_queue_index));

  // Test only disabled queues.
  std::vector<base::PendingTask> tasks = GetTasks(1);
  size_t queue_order[] = {0};
  PushTasks(tasks, queue_order);
  selector_.DisableQueue(0);
  EXPECT_FALSE(selector_.IsQueueEnabled(0));
  EXPECT_FALSE(selector_.SelectWorkQueueToService(&chosen_queue_index));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestDelay) {
  std::vector<base::PendingTask> tasks = GetTasks(5);
  tasks[0].delayed_run_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(200);
  tasks[3].delayed_run_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(1, 2, 4, 3, 0));
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestControlStarvesOthers) {
  std::vector<base::PendingTask> tasks = GetTasks(4);
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(3,
                             PrioritizingTaskQueueSelector::CONTROL_PRIORITY);
  selector_.SetQueuePriority(2, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
  selector_.SetQueuePriority(
      1, PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  for (int i = 0; i < 100; i++) {
    size_t chosen_queue_index = 0;
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_queue_index));
    EXPECT_EQ(3ul, chosen_queue_index);
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormal) {
  std::vector<base::PendingTask> tasks = GetTasks(3);
  size_t queue_order[] = {0, 1, 2};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(2, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
  selector_.SetQueuePriority(
      1, PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    size_t chosen_queue_index = 0;
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_queue_index));
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check high doesn't starve normal.
  EXPECT_GT(counts[2], counts[0]);  // Check high gets more chance to run.
  EXPECT_EQ(0ul, counts[1]);        // Check best effort is starved.
}

TEST_F(PrioritizingTaskQueueSelectorTest, TestBestEffortGetsStarved) {
  std::vector<base::PendingTask> tasks = GetTasks(2);
  size_t queue_order[] = {0, 1};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(
      0, PrioritizingTaskQueueSelector::BEST_EFFORT_PRIORITY);
  selector_.SetQueuePriority(1, PrioritizingTaskQueueSelector::NORMAL_PRIORITY);
  size_t chosen_queue_index = 0;
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_queue_index));
    EXPECT_EQ(1ul, chosen_queue_index);
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(1, PrioritizingTaskQueueSelector::HIGH_PRIORITY);
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_queue_index));
    EXPECT_EQ(1ul, chosen_queue_index);
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(1,
                             PrioritizingTaskQueueSelector::CONTROL_PRIORITY);
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_queue_index));
    EXPECT_EQ(1ul, chosen_queue_index);
    // Don't remove task from queue to simulate all queues still being full.
  }
}

}  // namespace scheduler
