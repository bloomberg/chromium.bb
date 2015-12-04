// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_sets.h"

#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace scheduler {
class TimeDomain;

namespace internal {

class TaskQueueSetsTest : public testing::Test {
 public:
  void SetUp() override {
    virtual_time_domain_ = make_scoped_ptr<VirtualTimeDomain>(
        new VirtualTimeDomain(nullptr, base::TimeTicks()));
    task_queue_sets_.reset(
        new TaskQueueSets(TaskQueueSets::TaskType::IMMEDIATE, kNumSets));
  }

 protected:
  enum {
    kNumSets = 5  // An arbitary choice.
  };

  TaskQueueImpl* NewTaskQueue(const char* queue_name) {
    scoped_refptr<internal::TaskQueueImpl> queue =
        make_scoped_refptr(new internal::TaskQueueImpl(
            nullptr, virtual_time_domain_.get(), TaskQueue::Spec(queue_name),
            "test", "test"));
    task_queues_.push_back(queue);
    return queue.get();
  }

  TaskQueueImpl::Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    TaskQueueImpl::Task fake_task(FROM_HERE, base::Closure(), base::TimeTicks(),
                                  0, true);
    fake_task.set_enqueue_order(enqueue_order);
    return fake_task;
  }

  scoped_ptr<VirtualTimeDomain> virtual_time_domain_;
  std::vector<scoped_refptr<internal::TaskQueueImpl>> task_queues_;
  scoped_ptr<TaskQueueSets> task_queue_sets_;
};

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_QueueEmpty) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  queue->set_task_queue_set_index(set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_FALSE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
}

TEST_F(TaskQueueSetsTest, OnPushQueue) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  size_t set = TaskQueue::NORMAL_PRIORITY;
  queue->set_task_queue_set_index(set);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_FALSE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));

  queue->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(10));
  task_queue_sets_->OnPushQueue(queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_SingleTaskInSet) {
  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  queue->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  queue->set_task_queue_set_index(set);
  task_queue_sets_->OnPushQueue(queue);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue2");
  size_t set = 2;
  queue1->set_task_queue_set_index(set);
  queue2->set_task_queue_set_index(set);
  queue3->set_task_queue_set_index(set);
  queue1->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  task_queue_sets_->OnPushQueue(queue1);
  task_queue_sets_->OnPushQueue(queue2);
  task_queue_sets_->OnPushQueue(queue3);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);
}

TEST_F(TaskQueueSetsTest, OnPopQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  size_t set = 3;
  queue1->set_task_queue_set_index(set);
  queue2->set_task_queue_set_index(set);
  queue3->set_task_queue_set_index(set);
  queue1->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(3));
  queue2->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(1));
  queue3->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  task_queue_sets_->OnPushQueue(queue1);
  task_queue_sets_->OnPushQueue(queue2);
  task_queue_sets_->OnPushQueue(queue3);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);

  queue2->PopTaskFromImmediateWorkQueueForTest();
  task_queue_sets_->OnPopQueue(queue2);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, OnPopQueue_QueueBecomesEmpty) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue");
  size_t set = 4;
  queue1->set_task_queue_set_index(set);
  queue2->set_task_queue_set_index(set);
  queue3->set_task_queue_set_index(set);
  queue1->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  task_queue_sets_->OnPushQueue(queue1);
  task_queue_sets_->OnPushQueue(queue2);
  task_queue_sets_->OnPushQueue(queue3);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);

  queue3->PopTaskFromImmediateWorkQueueForTest();
  task_queue_sets_->OnPopQueue(queue3);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest,
       GetOldestQueueInSet_MultipleAgesInSetIntegerRollover) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  size_t set = 0;
  queue1->set_task_queue_set_index(set);
  queue2->set_task_queue_set_index(set);
  queue3->set_task_queue_set_index(set);
  queue1->PushTaskOntoImmediateWorkQueueForTest(
      FakeTaskWithEnqueueOrder(0x7ffffff1));
  queue2->PushTaskOntoImmediateWorkQueueForTest(
      FakeTaskWithEnqueueOrder(0x7ffffff0));
  queue3->PushTaskOntoImmediateWorkQueueForTest(
      FakeTaskWithEnqueueOrder(-0x7ffffff1));
  task_queue_sets_->OnPushQueue(queue1);
  task_queue_sets_->OnPushQueue(queue2);
  task_queue_sets_->OnPushQueue(queue3);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet_RemoveQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  size_t set = 1;
  queue1->set_task_queue_set_index(set);
  queue2->set_task_queue_set_index(set);
  queue3->set_task_queue_set_index(set);
  queue1->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  task_queue_sets_->OnPushQueue(queue1);
  task_queue_sets_->OnPushQueue(queue2);
  task_queue_sets_->OnPushQueue(queue3);

  task_queue_sets_->RemoveQueue(queue3);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);
}

TEST_F(TaskQueueSetsTest, MoveQueue) {
  internal::TaskQueueImpl* queue1 = NewTaskQueue("queue1");
  internal::TaskQueueImpl* queue2 = NewTaskQueue("queue2");
  internal::TaskQueueImpl* queue3 = NewTaskQueue("queue3");
  internal::TaskQueueImpl* queue4 = NewTaskQueue("queue4");
  size_t set1 = 1;
  size_t set2 = 2;
  queue1->set_task_queue_set_index(set1);
  queue2->set_task_queue_set_index(set1);
  queue3->set_task_queue_set_index(set2);
  queue4->set_task_queue_set_index(set2);
  queue1->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(6));
  queue2->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(5));
  queue3->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(4));
  queue4->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_sets_->OnPushQueue(queue1);
  task_queue_sets_->OnPushQueue(queue2);
  task_queue_sets_->OnPushQueue(queue3);
  task_queue_sets_->OnPushQueue(queue4);

  internal::TaskQueueImpl* selected_queue;
  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set1, &selected_queue));
  EXPECT_EQ(queue2, selected_queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set2, &selected_queue));
  EXPECT_EQ(queue4, selected_queue);

  task_queue_sets_->MoveQueue(queue4, set2, set1);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set1, &selected_queue));
  EXPECT_EQ(queue4, selected_queue);

  EXPECT_TRUE(task_queue_sets_->GetOldestQueueInSet(set2, &selected_queue));
  EXPECT_EQ(queue3, selected_queue);
}

TEST_F(TaskQueueSetsTest, IsSetEmpty_NoWork) {
  size_t set = 0;
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));

  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  queue->set_task_queue_set_index(set);
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));
}

TEST_F(TaskQueueSetsTest, IsSetEmpty_Work) {
  size_t set = 0;
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));

  internal::TaskQueueImpl* queue = NewTaskQueue("queue");
  queue->set_task_queue_set_index(set);
  queue->PushTaskOntoImmediateWorkQueueForTest(FakeTaskWithEnqueueOrder(1));
  task_queue_sets_->OnPushQueue(queue);
  EXPECT_FALSE(task_queue_sets_->IsSetEmpty(set));

  queue->PopTaskFromImmediateWorkQueueForTest();
  task_queue_sets_->OnPopQueue(queue);
  EXPECT_TRUE(task_queue_sets_->IsSetEmpty(set));
}

}  // namespace internal
}  // namespace scheduler
