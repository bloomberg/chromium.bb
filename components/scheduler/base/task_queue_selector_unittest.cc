// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_selector.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/pending_task.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace scheduler {
namespace internal {

class MockObserver : public TaskQueueSelector::Observer {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}

  MOCK_METHOD1(OnTaskQueueEnabled, void(internal::TaskQueueImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class TaskQueueSelectorTest : public testing::Test {
 public:
  TaskQueueSelectorTest()
      : test_closure_(base::Bind(&TaskQueueSelectorTest::TestFunction)) {}
  ~TaskQueueSelectorTest() override {}

  std::vector<TaskQueueImpl::Task> GetTasks(int count) {
    std::vector<TaskQueueImpl::Task> tasks;
    for (int i = 0; i < count; i++) {
      TaskQueueImpl::Task task(FROM_HERE, test_closure_, 0, true);
      task.set_enqueue_order(i);
      tasks.push_back(task);
    }
    return tasks;
  }

  void PushTasks(const std::vector<TaskQueueImpl::Task>& tasks,
                 const size_t queue_indices[]) {
    std::set<size_t> changed_queue_set;
    for (size_t i = 0; i < tasks.size(); i++) {
      changed_queue_set.insert(queue_indices[i]);
      task_queues_[queue_indices[i]]->PushTaskOntoWorkQueueForTest(tasks[i]);
    }
    for (size_t queue_index : changed_queue_set) {
      selector_.GetTaskQueueSets()->OnPushQueue(
          task_queues_[queue_index].get());
    }
  }

  std::vector<size_t> PopTasks() {
    std::vector<size_t> order;
    TaskQueueImpl* chosen_queue;
    while (selector_.SelectQueueToService(&chosen_queue)) {
      size_t chosen_queue_index =
          queue_to_index_map_.find(chosen_queue)->second;
      order.push_back(chosen_queue_index);
      chosen_queue->PopTaskFromWorkQueueForTest();
      selector_.GetTaskQueueSets()->OnPopQueue(chosen_queue);
    }
    return order;
  }

  static void TestFunction() {}

 protected:
  void SetUp() final {
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      scoped_refptr<TaskQueueImpl> task_queue =
          make_scoped_refptr(new TaskQueueImpl(
              nullptr, TaskQueue::Spec("test queue"), "test", "test"));
      selector_.AddQueue(task_queue.get());
      task_queues_.push_back(task_queue);
    }
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      EXPECT_TRUE(selector_.IsQueueEnabled(task_queues_[i].get())) << i;
      queue_to_index_map_.insert(std::make_pair(task_queues_[i].get(), i));
    }
  }

  const size_t kTaskQueueCount = 5;
  base::Closure test_closure_;
  TaskQueueSelector selector_;
  std::vector<scoped_refptr<TaskQueueImpl>> task_queues_;
  std::map<TaskQueueImpl*, size_t> queue_to_index_map_;
};

TEST_F(TaskQueueSelectorTest, TestDefaultPriority) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(5);
  size_t queue_order[] = {4, 3, 2, 1, 0};
  PushTasks(tasks, queue_order);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4, 3, 2, 1, 0));
}

TEST_F(TaskQueueSelectorTest, TestHighPriority) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(TaskQueueSelectorTest, TestBestEffortPriority) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2, 1, 3, 4, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlPriority) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[4].get(),
                             TaskQueue::CONTROL_PRIORITY);
  EXPECT_TRUE(selector_.IsQueueEnabled(task_queues_[4].get()));
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  EXPECT_TRUE(selector_.IsQueueEnabled(task_queues_[2].get()));
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4, 2, 0, 1, 3));
}

TEST_F(TaskQueueSelectorTest, TestObserverWithSetQueuePriority) {
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::DISABLED_PRIORITY);
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(1);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::NORMAL_PRIORITY);
}

TEST_F(TaskQueueSelectorTest,
       TestObserverWithSetQueuePriorityAndQueueAlreadyEnabed) {
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::NORMAL_PRIORITY);
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(0);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::NORMAL_PRIORITY);
}

TEST_F(TaskQueueSelectorTest, TestDisableEnable) {
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);

  std::vector<TaskQueueImpl::Task> tasks = GetTasks(5);
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::DISABLED_PRIORITY);
  EXPECT_FALSE(selector_.IsQueueEnabled(task_queues_[2].get()));
  selector_.SetQueuePriority(task_queues_[4].get(),
                             TaskQueue::DISABLED_PRIORITY);
  EXPECT_FALSE(selector_.IsQueueEnabled(task_queues_[4].get()));
  EXPECT_THAT(PopTasks(), testing::ElementsAre(0, 1, 3));

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(2);
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2));
  selector_.SetQueuePriority(task_queues_[4].get(), TaskQueue::NORMAL_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4));
}

TEST_F(TaskQueueSelectorTest, TestEmptyQueues) {
  TaskQueueImpl* chosen_queue = nullptr;
  EXPECT_FALSE(selector_.SelectQueueToService(&chosen_queue));

  // Test only disabled queues.
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(1);
  size_t queue_order[] = {0};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::DISABLED_PRIORITY);
  EXPECT_FALSE(selector_.IsQueueEnabled(task_queues_[0].get()));
  EXPECT_FALSE(selector_.SelectQueueToService(&chosen_queue));
}

TEST_F(TaskQueueSelectorTest, TestAge) {
  std::vector<TaskQueueImpl::Task> tasks;
  int enqueue_order[] = {10, 1, 2, 9, 4};
  for (int i = 0; i < 5; i++) {
    TaskQueueImpl::Task task(FROM_HERE, test_closure_, 0, true);
    task.set_enqueue_order(enqueue_order[i]);
    tasks.push_back(task);
  }
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(tasks, queue_order);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(1, 2, 4, 3, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlStarvesOthers) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(4);
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[3].get(),
                             TaskQueue::CONTROL_PRIORITY);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  for (int i = 0; i < 100; i++) {
    TaskQueueImpl* chosen_queue = nullptr;
    EXPECT_TRUE(selector_.SelectQueueToService(&chosen_queue));
    EXPECT_EQ(task_queues_[3].get(), chosen_queue);
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormal) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(3);
  size_t queue_order[] = {0, 1, 2};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    TaskQueueImpl* chosen_queue = nullptr;
    EXPECT_TRUE(selector_.SelectQueueToService(&chosen_queue));
    size_t chosen_queue_index = queue_to_index_map_.find(chosen_queue)->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check high doesn't starve normal.
  EXPECT_GT(counts[2], counts[0]);  // Check high gets more chance to run.
  EXPECT_EQ(0ul, counts[1]);        // Check best effort is starved.
}

TEST_F(TaskQueueSelectorTest, TestBestEffortGetsStarved) {
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(2);
  size_t queue_order[] = {0, 1};
  PushTasks(tasks, queue_order);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::NORMAL_PRIORITY);
  TaskQueueImpl* chosen_queue = nullptr;
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectQueueToService(&chosen_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_queue);
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::HIGH_PRIORITY);
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectQueueToService(&chosen_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_queue);
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::CONTROL_PRIORITY);
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectQueueToService(&chosen_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_queue);
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, DisabledPriorityIsPenultimate) {
  EXPECT_EQ(TaskQueue::QUEUE_PRIORITY_COUNT, TaskQueue::DISABLED_PRIORITY + 1);
}

TEST_F(TaskQueueSelectorTest, EnabledWorkQueuesEmpty) {
  EXPECT_TRUE(selector_.EnabledWorkQueuesEmpty());
  std::vector<TaskQueueImpl::Task> tasks = GetTasks(2);
  size_t queue_order[] = {0, 1};
  PushTasks(tasks, queue_order);

  EXPECT_FALSE(selector_.EnabledWorkQueuesEmpty());
  PopTasks();
  EXPECT_TRUE(selector_.EnabledWorkQueuesEmpty());
}

}  // namespace internal
}  // namespace scheduler
