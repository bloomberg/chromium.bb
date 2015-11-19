// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_sets.h"

#include "components/scheduler/base/task_queue_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace scheduler {
namespace internal {

class TaskQueueSetsTest : public testing::Test {
 public:
  void SetUp() override { task_queue_sets_.reset(new TaskQueueSets(kNumSets)); }

 protected:
  enum {
    kNumSets = 5  // An arbitary choice.
  };

  TaskQueueImpl* NewTaskQueue(const char* queue_name) {
    scoped_refptr<internal::TaskQueueImpl> queue =
        make_scoped_refptr(new internal::TaskQueueImpl(
            nullptr, TaskQueue::Spec(queue_name), "test", "test"));
    task_queues_.push_back(queue);
    return queue.get();
  }

  TaskQueueImpl::Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    TaskQueueImpl::Task fake_task(FROM_HERE, base::Closure(), 0, true);
    fake_task.set_enqueue_order(enqueue_order);
    return fake_task;
  }

  std::vector<scoped_refptr<internal::TaskQueueImpl>> task_queues_;
  scoped_ptr<TaskQueueSets> task_queue_sets_;
};

TEST_F(TaskQueueSetsTest, AssignQueueToSet) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  task_queue_sets_->AssignQueueToSet(queue, set);

  EXPECT_EQ(set, queue->get_task_queue_set_index());
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_QueueEmpty) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  task_queue_sets_->AssignQueueToSet(queue, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_FALSE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
}

TEST_F(TaskQueueSetsTest, OnPushQueue) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  task_queue_sets_->AssignQueueToSet(queue, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_FALSE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));

  queue->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(10));
  task_queue_sets_->OnPushQueue(queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_SingleTaskInSet) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  queue->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  task_queue_sets_->AssignQueueToSet(queue, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue2");
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  size_t set = 2;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);
}

TEST_F(TaskQueueSetsTest, OnPopQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(3));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(1));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  size_t set = 3;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);

  queue2->PopTaskFromWorkQueueForTest();
  task_queue_sets_->OnPopQueue(queue2);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, OnPopQueue_QueueBecomesEmpty) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue");
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);

  queue3->PopTaskFromWorkQueueForTest();
  task_queue_sets_->OnPopQueue(queue3);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest,
       GetOldestQueueInSet_MultipleAgesInSetIntegerRollover) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(0x7ffffff1));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(0x7ffffff0));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(-0x7ffffff1));
  size_t set = 0;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet_RemoveQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  size_t set = 1;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);
  task_queue_sets_->RemoveQueue(queue3);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, AssignQueueToSet_Complex) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  internal::TaskQueueImpl* queue4 = NewTaskQueue("queue4");
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  queue4->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(3));
  size_t set1 = 1;
  size_t set2 = 2;
  task_queue_sets_->AssignQueueToSet(queue1, set1);
  task_queue_sets_->AssignQueueToSet(queue2, set1);
  task_queue_sets_->AssignQueueToSet(queue3, set2);
  task_queue_sets_->AssignQueueToSet(queue4, set2);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set1, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set2, &selected_queue));
  EXPECT_EQ(queue4, selected_queue);

  task_queue_sets_->AssignQueueToSet(queue4, set1);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set1, &selected_queue));
  EXPECT_EQ(queue4, selected_queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set2, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);
}

TEST_F(TaskQueueSetsTest, IsSetEmpty_NoWork) {
  size_t set = 0;
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));

  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  task_queue_sets_->AssignQueueToSet(queue, set);
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));
}

TEST_F(TaskQueueSetsTest, IsSetEmpty_Work) {
  size_t set = 0;
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));

  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  queue->PushTaskOntoWorkQueueForTest(FakeTaskWithEnqueueOrder(1));
  task_queue_sets_->AssignQueueToSet(queue, set);
  EXPECT_FALSE(task_queue_sets_->IsSetEmpty(set));

  queue->PopTaskFromWorkQueueForTest();
  task_queue_sets_->OnPopQueue(queue);
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));
}

}  // namespace internal
}  // namespace scheduler
