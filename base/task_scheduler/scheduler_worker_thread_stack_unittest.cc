// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_thread_stack.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task_scheduler/scheduler_worker_thread.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/task_scheduler/test_utils.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class MockSchedulerWorkerThreadDelegate
    : public SchedulerWorkerThread::Delegate {
 public:
  void OnMainEntry() override {}
  scoped_refptr<Sequence> GetWork(
      SchedulerWorkerThread* worker_thread) override {
    return nullptr;
  }
  void EnqueueSequence(scoped_refptr<Sequence> sequence) override {
    NOTREACHED();
  }
};

class TaskSchedulerWorkerThreadStackTest : public testing::Test {
 protected:
  void SetUp() override {
    thread_a_ = SchedulerWorkerThread::CreateSchedulerWorkerThread(
        ThreadPriority::NORMAL, &delegate_, &task_tracker_);
    ASSERT_TRUE(thread_a_);
    thread_b_ = SchedulerWorkerThread::CreateSchedulerWorkerThread(
        ThreadPriority::NORMAL, &delegate_, &task_tracker_);
    ASSERT_TRUE(thread_b_);
    thread_c_ = SchedulerWorkerThread::CreateSchedulerWorkerThread(
        ThreadPriority::NORMAL, &delegate_, &task_tracker_);
    ASSERT_TRUE(thread_c_);
  }

  void TearDown() override {
    thread_a_->JoinForTesting();
    thread_b_->JoinForTesting();
    thread_c_->JoinForTesting();
  }

  std::unique_ptr<SchedulerWorkerThread> thread_a_;
  std::unique_ptr<SchedulerWorkerThread> thread_b_;
  std::unique_ptr<SchedulerWorkerThread> thread_c_;

 private:
  MockSchedulerWorkerThreadDelegate delegate_;
  TaskTracker task_tracker_;
};

}  // namespace

// Verify that Push() and Pop() add/remove values in FIFO order.
TEST_F(TaskSchedulerWorkerThreadStackTest, PushPop) {
  SchedulerWorkerThreadStack stack;
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(thread_a_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(thread_b_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(thread_c_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(thread_c_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(thread_c_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(thread_c_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(thread_b_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(thread_a_.get(), stack.Pop());
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());
}

// Verify that a value can be removed by Remove().
TEST_F(TaskSchedulerWorkerThreadStackTest, Remove) {
  SchedulerWorkerThreadStack stack;
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(thread_a_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(thread_b_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(thread_c_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(3U, stack.Size());

  stack.Remove(thread_b_.get());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(thread_c_.get(), stack.Pop());
  EXPECT_FALSE(stack.IsEmpty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(thread_a_.get(), stack.Pop());
  EXPECT_TRUE(stack.IsEmpty());
  EXPECT_EQ(0U, stack.Size());
}

// Verify that a value can be pushed again after it has been removed.
TEST_F(TaskSchedulerWorkerThreadStackTest, PushAfterRemove) {
  SchedulerWorkerThreadStack stack;
  EXPECT_EQ(0U, stack.Size());
  EXPECT_TRUE(stack.IsEmpty());

  stack.Push(thread_a_.get());
  EXPECT_EQ(1U, stack.Size());
  EXPECT_FALSE(stack.IsEmpty());

  stack.Remove(thread_a_.get());
  EXPECT_EQ(0U, stack.Size());
  EXPECT_TRUE(stack.IsEmpty());

  stack.Push(thread_a_.get());
  EXPECT_EQ(1U, stack.Size());
  EXPECT_FALSE(stack.IsEmpty());
}

// Verify that Push() DCHECKs when a value is inserted twice.
TEST_F(TaskSchedulerWorkerThreadStackTest, PushTwice) {
  SchedulerWorkerThreadStack stack;
  stack.Push(thread_a_.get());
  EXPECT_DCHECK_DEATH({ stack.Push(thread_a_.get()); }, "");
}

}  // namespace internal
}  // namespace base
