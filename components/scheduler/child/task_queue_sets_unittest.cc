// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/task_queue_sets.h"

#include "components/scheduler/child/task_queue_impl.h"
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

  internal::TaskQueueImpl* NewTaskQueue() {
    scoped_refptr<internal::TaskQueueImpl> queue =
        make_scoped_refptr(new internal::TaskQueueImpl(
            nullptr, TaskQueue::Spec("test queue"), "test", "test"));
    task_queues_.push_back(queue);
    return queue.get();
  }

  base::PendingTask FakeTaskWithSequenceNum(int sequence_num) {
    base::PendingTask fake_task(FROM_HERE, base::Closure());
    fake_task.sequence_num = sequence_num;
    return fake_task;
  }

  std::vector<scoped_refptr<internal::TaskQueueImpl>> task_queues_;
  scoped_ptr<TaskQueueSets> task_queue_sets_;
};

TEST_F(TaskQueueSetsTest, AssignQueueToSet) {
  internal::TaskQueueImpl* queue = NewTaskQueue();
  size_t set = TaskQueue::NORMAL_PRIORITY;
  task_queue_sets_->AssignQueueToSet(queue, set);

  EXPECT_EQ(set, queue->get_task_queue_set_index());
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_QueueEmpty) {
  internal::TaskQueueImpl* queue = NewTaskQueue();
  size_t set = TaskQueue::NORMAL_PRIORITY;
  task_queue_sets_->AssignQueueToSet(queue, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_FALSE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
}

TEST_F(TaskQueueSetsTest, OnPushQueue) {
  internal::TaskQueueImpl* queue = NewTaskQueue();
  size_t set = TaskQueue::NORMAL_PRIORITY;
  task_queue_sets_->AssignQueueToSet(queue, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_FALSE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));

  queue->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(10));
  task_queue_sets_->OnPushQueue(queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_SingleTaskInSet) {
  internal::TaskQueueImpl* queue = NewTaskQueue();
  queue->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(10));
  size_t set = 1;
  task_queue_sets_->AssignQueueToSet(queue, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue();
  internal::TaskQueueImpl* queue2 = NewTaskQueue();
  internal::TaskQueueImpl* queue3 = NewTaskQueue();
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(4));
  size_t set = 2;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);
}

TEST_F(TaskQueueSetsTest, OnPopQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue();
  internal::TaskQueueImpl* queue2 = NewTaskQueue();
  internal::TaskQueueImpl* queue3 = NewTaskQueue();
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(3));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(1));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(4));
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
  internal::TaskQueueImpl* queue1 = NewTaskQueue();
  internal::TaskQueueImpl* queue2 = NewTaskQueue();
  internal::TaskQueueImpl* queue3 = NewTaskQueue();
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(4));
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
  internal::TaskQueueImpl* queue1 = NewTaskQueue();
  internal::TaskQueueImpl* queue2 = NewTaskQueue();
  internal::TaskQueueImpl* queue3 = NewTaskQueue();
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(0x7ffffff1));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(0x7ffffff0));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(-0x7ffffff1));
  size_t set = 0;
  task_queue_sets_->AssignQueueToSet(queue1, set);
  task_queue_sets_->AssignQueueToSet(queue2, set);
  task_queue_sets_->AssignQueueToSet(queue3, set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet_RemoveQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue();
  internal::TaskQueueImpl* queue2 = NewTaskQueue();
  internal::TaskQueueImpl* queue3 = NewTaskQueue();
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(4));
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
  internal::TaskQueueImpl* queue1 = NewTaskQueue();
  internal::TaskQueueImpl* queue2 = NewTaskQueue();
  internal::TaskQueueImpl* queue3 = NewTaskQueue();
  internal::TaskQueueImpl* queue4 = NewTaskQueue();
  queue1->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(6));
  queue2->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(5));
  queue3->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(4));
  queue4->PushTaskOntoWorkQueueForTest(FakeTaskWithSequenceNum(3));
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

}  // namespace internal
}  // namespace scheduler
