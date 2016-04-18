// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/utils.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/priority_queue.h"
#include "base/task_scheduler/scheduler_task_executor.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

bool operator==(const Closure& lhs, const Closure& rhs) {
  return lhs.Equals(rhs);
}

namespace internal {
namespace {

class MockSchedulerTaskExecutor : public SchedulerTaskExecutor {
 public:
  void PostTaskWithSequence(std::unique_ptr<Task> task,
                            scoped_refptr<Sequence> sequence) override {
    PostTaskWithSequenceMock(task->task, task->traits, sequence.get());
  }

  MOCK_METHOD3(PostTaskWithSequenceMock,
               void(const Closure&, const TaskTraits&, Sequence* sequence));
};

// Verifies that when PostTaskToExecutor receives a Task that is allowed to be
// posted, it forwards it to a SchedulerTaskExecutor.
TEST(TaskSchedulerPostTaskToExecutorTest, PostTaskAllowed) {
  const Closure closure(Bind(&DoNothing));
  const TaskTraits traits;
  scoped_refptr<Sequence> sequence(new Sequence);
  testing::StrictMock<MockSchedulerTaskExecutor> executor;
  TaskTracker task_tracker;

  EXPECT_CALL(executor,
              PostTaskWithSequenceMock(closure, traits, sequence.get()));
  PostTaskToExecutor(FROM_HERE, closure, traits, TimeDelta(), sequence,
                     &executor, &task_tracker);
}

// Verifies that when PostTaskToExecutor receives a Task that isn't allowed to
// be posted, it doesn't forward it to a SchedulerTaskExecutor.
TEST(TaskSchedulerPostTaskToExecutorTest, PostTaskNotAllowed) {
  // Use a strict mock to ensure that the test fails when there is an unexpected
  // call to the mock method of |executor|.
  testing::StrictMock<MockSchedulerTaskExecutor> executor;
  TaskTracker task_tracker;
  task_tracker.Shutdown();

  PostTaskToExecutor(
      FROM_HERE, Bind(&DoNothing),
      TaskTraits().WithShutdownBehavior(TaskShutdownBehavior::SKIP_ON_SHUTDOWN),
      TimeDelta(), make_scoped_refptr(new Sequence), &executor, &task_tracker);
}

// Verifies that when AddTaskToSequenceAndPriorityQueue is called with an empty
// sequence, the task is added to the sequence and the sequence is added to the
// priority queue.
TEST(TaskSchedulerAddTaskToSequenceAndPriorityQueueTest,
     PostTaskInEmptySequence) {
  std::unique_ptr<Task> task(
      new Task(FROM_HERE, Bind(&DoNothing), TaskTraits(), TimeTicks()));
  const Task* task_raw = task.get();
  scoped_refptr<Sequence> sequence(new Sequence);
  PriorityQueue priority_queue;

  // Post |task|.
  EXPECT_TRUE(AddTaskToSequenceAndPriorityQueue(std::move(task), sequence,
                                                &priority_queue));

  // Expect to find the sequence in the priority queue.
  EXPECT_EQ(sequence, priority_queue.BeginTransaction()->Peek().sequence);

  // Expect to find |task| alone in |sequence|.
  EXPECT_EQ(task_raw, sequence->PeekTask());
  sequence->PopTask();
  EXPECT_EQ(nullptr, sequence->PeekTask());
}

// Verifies that when AddTaskToSequenceAndPriorityQueue is called with a
// sequence that already contains a task, the task is added to the sequence but
// the sequence is not added to the priority queue.
TEST(TaskSchedulerAddTaskToSequenceAndPriorityQueueTest,
     PostTaskInNonEmptySequence) {
  std::unique_ptr<Task> task(
      new Task(FROM_HERE, Bind(&DoNothing), TaskTraits(), TimeTicks()));
  const Task* task_raw = task.get();
  scoped_refptr<Sequence> sequence(new Sequence);
  PriorityQueue priority_queue;

  // Add an initial task in |sequence|.
  sequence->PushTask(WrapUnique(
      new Task(FROM_HERE, Bind(&DoNothing), TaskTraits(), TimeTicks())));

  // Post |task|.
  EXPECT_FALSE(AddTaskToSequenceAndPriorityQueue(std::move(task), sequence,
                                                 &priority_queue));

  // Expect to find the priority queue empty.
  EXPECT_TRUE(priority_queue.BeginTransaction()->Peek().is_null());

  // Expect to find |task| in |sequence| behind the initial task.
  EXPECT_NE(task_raw, sequence->PeekTask());
  sequence->PopTask();
  EXPECT_EQ(task_raw, sequence->PeekTask());
  sequence->PopTask();
  EXPECT_EQ(nullptr, sequence->PeekTask());
}

}  // namespace
}  // namespace internal
}  // namespace base
