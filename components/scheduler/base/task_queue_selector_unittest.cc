// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_selector.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/pending_task.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/base/work_queue.h"
#include "components/scheduler/base/work_queue_sets.h"
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

class TaskQueueSelectorForTest : public TaskQueueSelector {
 public:
  using TaskQueueSelector::ChooseOldestWithPriority;
  using TaskQueueSelector::SetImmediateStarvationCountForTest;
};

class TaskQueueSelectorTest : public testing::Test {
 public:
  TaskQueueSelectorTest()
      : test_closure_(base::Bind(&TaskQueueSelectorTest::TestFunction)) {}
  ~TaskQueueSelectorTest() override {}

  void PushTasks(const size_t queue_indices[], size_t num_tasks) {
    std::set<size_t> changed_queue_set;
    for (size_t i = 0; i < num_tasks; i++) {
      changed_queue_set.insert(queue_indices[i]);
      task_queues_[queue_indices[i]]->immediate_work_queue()->PushTaskForTest(
          TaskQueueImpl::Task(FROM_HERE, test_closure_, base::TimeTicks(), 0,
                              true, i));
    }
    for (size_t queue_index : changed_queue_set) {
      selector_.immediate_task_queue_sets()->OnPushQueue(
          task_queues_[queue_index]->immediate_work_queue());
    }
  }

  void PushTasksWithEnqueueOrder(const size_t queue_indices[],
                                 const size_t enqueue_orders[],
                                 size_t num_tasks) {
    std::set<size_t> changed_queue_set;
    for (size_t i = 0; i < num_tasks; i++) {
      changed_queue_set.insert(queue_indices[i]);
      task_queues_[queue_indices[i]]->immediate_work_queue()->PushTaskForTest(
          TaskQueueImpl::Task(FROM_HERE, test_closure_, base::TimeTicks(), 0,
                              true, enqueue_orders[i]));
    }
    for (size_t queue_index : changed_queue_set) {
      selector_.immediate_task_queue_sets()->OnPushQueue(
          task_queues_[queue_index]->immediate_work_queue());
    }
  }

  std::vector<size_t> PopTasks() {
    std::vector<size_t> order;
    WorkQueue* chosen_work_queue;
    while (selector_.SelectWorkQueueToService(&chosen_work_queue)) {
      size_t chosen_queue_index =
          queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
      order.push_back(chosen_queue_index);
      chosen_work_queue->PopTaskForTest();
      selector_.immediate_task_queue_sets()->OnPopQueue(chosen_work_queue);
    }
    return order;
  }

  static void TestFunction() {}

 protected:
  void SetUp() final {
    virtual_time_domain_ = make_scoped_ptr<VirtualTimeDomain>(
        new VirtualTimeDomain(nullptr, base::TimeTicks()));
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      scoped_refptr<TaskQueueImpl> task_queue = make_scoped_refptr(
          new TaskQueueImpl(nullptr, virtual_time_domain_.get(),
                            TaskQueue::Spec("test queue"), "test", "test"));
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
  TaskQueueSelectorForTest selector_;
  scoped_ptr<VirtualTimeDomain> virtual_time_domain_;
  std::vector<scoped_refptr<TaskQueueImpl>> task_queues_;
  std::map<TaskQueueImpl*, size_t> queue_to_index_map_;
};

TEST_F(TaskQueueSelectorTest, TestDefaultPriority) {
  size_t queue_order[] = {4, 3, 2, 1, 0};
  PushTasks(queue_order, 5);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(4, 3, 2, 1, 0));
}

TEST_F(TaskQueueSelectorTest, TestHighPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(TaskQueueSelectorTest, TestBestEffortPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(2, 1, 3, 4, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
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

  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
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
  WorkQueue* chosen_work_queue = nullptr;
  EXPECT_FALSE(selector_.SelectWorkQueueToService(&chosen_work_queue));

  // Test only disabled queues.
  size_t queue_order[] = {0};
  PushTasks(queue_order, 1);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::DISABLED_PRIORITY);
  EXPECT_FALSE(selector_.IsQueueEnabled(task_queues_[0].get()));
  EXPECT_FALSE(selector_.SelectWorkQueueToService(&chosen_work_queue));
}

TEST_F(TaskQueueSelectorTest, TestAge) {
  size_t enqueue_order[] = {10, 1, 2, 9, 4};
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasksWithEnqueueOrder(queue_order, enqueue_order, 5);
  EXPECT_THAT(PopTasks(), testing::ElementsAre(1, 2, 4, 3, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlStarvesOthers) {
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(queue_order, 4);
  selector_.SetQueuePriority(task_queues_[3].get(),
                             TaskQueue::CONTROL_PRIORITY);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = nullptr;
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[3].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormal) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::HIGH_PRIORITY);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = nullptr;
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check high doesn't starve normal.
  EXPECT_GT(counts[2], counts[0]);  // Check high gets more chance to run.
  EXPECT_EQ(0ul, counts[1]);        // Check best effort is starved.
}

TEST_F(TaskQueueSelectorTest, TestBestEffortGetsStarved) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::BEST_EFFORT_PRIORITY);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::NORMAL_PRIORITY);
  WorkQueue* chosen_work_queue = nullptr;
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::HIGH_PRIORITY);
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::CONTROL_PRIORITY);
  for (int i = 0; i < 100; i++) {
    EXPECT_TRUE(selector_.SelectWorkQueueToService(&chosen_work_queue));
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, DisabledPriorityIsPenultimate) {
  EXPECT_EQ(TaskQueue::QUEUE_PRIORITY_COUNT, TaskQueue::DISABLED_PRIORITY + 1);
}

TEST_F(TaskQueueSelectorTest, EnabledWorkQueuesEmpty) {
  EXPECT_TRUE(selector_.EnabledWorkQueuesEmpty());
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);

  EXPECT_FALSE(selector_.EnabledWorkQueuesEmpty());
  PopTasks();
  EXPECT_TRUE(selector_.EnabledWorkQueuesEmpty());
}

TEST_F(TaskQueueSelectorTest, ChooseOldestWithPriority_Empty) {
  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_FALSE(selector_.ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY,
                                                  &chose_delayed_over_immediate,
                                                  &chosen_work_queue));
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, ChooseOldestWithPriority_OnlyDelayed) {
  task_queues_[0]->delayed_work_queue()->PushTaskForTest(TaskQueueImpl::Task(
      FROM_HERE, test_closure_, base::TimeTicks(), 0, true, 0));
  selector_.delayed_task_queue_sets()->OnPushQueue(
      task_queues_[0]->delayed_work_queue());

  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_TRUE(selector_.ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY,
                                                 &chose_delayed_over_immediate,
                                                 &chosen_work_queue));
  EXPECT_EQ(chosen_work_queue, task_queues_[0]->delayed_work_queue());
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, ChooseOldestWithPriority_OnlyImmediate) {
  task_queues_[0]->immediate_work_queue()->PushTaskForTest(TaskQueueImpl::Task(
      FROM_HERE, test_closure_, base::TimeTicks(), 0, true, 0));
  selector_.immediate_task_queue_sets()->OnPushQueue(
      task_queues_[0]->immediate_work_queue());

  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_TRUE(selector_.ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY,
                                                 &chose_delayed_over_immediate,
                                                 &chosen_work_queue));
  EXPECT_EQ(chosen_work_queue, task_queues_[0]->immediate_work_queue());
  EXPECT_FALSE(chose_delayed_over_immediate);
}

struct ChooseOldestWithPriorityTestParam {
  int delayed_task_enqueue_order;
  int immediate_task_enqueue_order;
  int immediate_starvation_count;
  const char* expected_work_queue_name;
  bool expected_did_starve_immediate_queue;
};

static const ChooseOldestWithPriorityTestParam
    kChooseOldestWithPriorityTestCases[] = {
        {1, 2, 0, "delayed", true},
        {1, 2, 1, "delayed", true},
        {1, 2, 2, "delayed", true},
        {1, 2, 3, "immediate", false},
        {1, 2, 4, "immediate", false},
        {2, 1, 4, "immediate", false},
        {2, 1, 4, "immediate", false},
};

class ChooseOldestWithPriorityTest
    : public TaskQueueSelectorTest,
      public testing::WithParamInterface<ChooseOldestWithPriorityTestParam> {};

TEST_P(ChooseOldestWithPriorityTest, RoundRobinTest) {
  task_queues_[0]->immediate_work_queue()->PushTaskForTest(
      TaskQueueImpl::Task(FROM_HERE, test_closure_, base::TimeTicks(),
                          GetParam().immediate_task_enqueue_order, true,
                          GetParam().immediate_task_enqueue_order));
  selector_.immediate_task_queue_sets()->OnPushQueue(
      task_queues_[0]->immediate_work_queue());

  task_queues_[0]->delayed_work_queue()->PushTaskForTest(
      TaskQueueImpl::Task(FROM_HERE, test_closure_, base::TimeTicks(),
                          GetParam().delayed_task_enqueue_order, true,
                          GetParam().delayed_task_enqueue_order));
  selector_.delayed_task_queue_sets()->OnPushQueue(
      task_queues_[0]->delayed_work_queue());

  selector_.SetImmediateStarvationCountForTest(
      GetParam().immediate_starvation_count);

  WorkQueue* chosen_work_queue = nullptr;
  bool chose_delayed_over_immediate = false;
  EXPECT_TRUE(selector_.ChooseOldestWithPriority(TaskQueue::NORMAL_PRIORITY,
                                                 &chose_delayed_over_immediate,
                                                 &chosen_work_queue));
  EXPECT_EQ(chosen_work_queue->task_queue(), task_queues_[0].get());
  EXPECT_STREQ(chosen_work_queue->name(), GetParam().expected_work_queue_name);
  EXPECT_EQ(chose_delayed_over_immediate,
            GetParam().expected_did_starve_immediate_queue);
}

INSTANTIATE_TEST_CASE_P(ChooseOldestWithPriorityTest,
                        ChooseOldestWithPriorityTest,
                        testing::ValuesIn(kChooseOldestWithPriorityTestCases));

}  // namespace internal
}  // namespace scheduler
