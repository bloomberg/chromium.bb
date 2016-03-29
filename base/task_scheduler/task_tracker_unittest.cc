// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/task_tracker.h"

#include <queue>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/task_scheduler/test_utils.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

// Calls TaskTracker::Shutdown() asynchronously.
class ThreadCallingShutdown : public SimpleThread {
 public:
  explicit ThreadCallingShutdown(TaskTracker* tracker)
      : SimpleThread("ThreadCallingShutdown"),
        tracker_(tracker),
        has_returned_(true, false) {}

  // Returns true once the async call to Shutdown() has returned.
  bool has_returned() { return has_returned_.IsSignaled(); }

 private:
  void Run() override {
    tracker_->Shutdown();
    has_returned_.Signal();
  }

  TaskTracker* const tracker_;
  WaitableEvent has_returned_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCallingShutdown);
};

// Runs a task asynchronously.
class ThreadRunningTask : public SimpleThread {
 public:
  explicit ThreadRunningTask(TaskTracker* tracker, const Task* task)
      : SimpleThread("ThreadRunningTask"), tracker_(tracker), task_(task) {}

 private:
  void Run() override { tracker_->RunTask(task_); }

  TaskTracker* const tracker_;
  const Task* const task_;

  DISALLOW_COPY_AND_ASSIGN(ThreadRunningTask);
};

class TaskSchedulerTaskTrackerTest
    : public testing::TestWithParam<TaskShutdownBehavior> {
 protected:
  TaskSchedulerTaskTrackerTest() = default;

  // Creates a task with |shutdown_behavior|.
  scoped_ptr<Task> CreateTask(TaskShutdownBehavior shutdown_behavior) {
    return make_scoped_ptr(new Task(
        FROM_HERE,
        Bind(&TaskSchedulerTaskTrackerTest::RunTaskCallback, Unretained(this)),
        TaskTraits().WithShutdownBehavior(shutdown_behavior)));
  }

  // Tries to post |task| via |tracker_|. If |tracker_| approves the operation,
  // |task| is added to |posted_tasks_|.
  void PostTaskViaTracker(scoped_ptr<Task> task) {
    tracker_.PostTask(
        Bind(&TaskSchedulerTaskTrackerTest::PostTaskCallback, Unretained(this)),
        std::move(task));
  }

  // Tries to run the next task in |posted_tasks_| via |tracker_|.
  void RunNextPostedTaskViaTracker() {
    ASSERT_FALSE(posted_tasks_.empty());
    tracker_.RunTask(posted_tasks_.front().get());
    posted_tasks_.pop();
  }

  // Calls tracker_->Shutdown() on a new thread. When this returns, Shutdown()
  // method has been entered on the new thread, but it hasn't necessarily
  // returned.
  void CallShutdownAsync() {
    ASSERT_FALSE(thread_calling_shutdown_);
    thread_calling_shutdown_.reset(new ThreadCallingShutdown(&tracker_));
    thread_calling_shutdown_->Start();
    while (!tracker_.IsShuttingDownForTesting() &&
           !tracker_.shutdown_completed()) {
      PlatformThread::YieldCurrentThread();
    }
  }

  void WaitForAsyncShutdownCompleted() {
    ASSERT_TRUE(thread_calling_shutdown_);
    thread_calling_shutdown_->Join();
    EXPECT_TRUE(thread_calling_shutdown_->has_returned());
    EXPECT_TRUE(tracker_.shutdown_completed());
  }

  void VerifyAsyncShutdownInProgress() {
    ASSERT_TRUE(thread_calling_shutdown_);
    EXPECT_FALSE(thread_calling_shutdown_->has_returned());
    EXPECT_FALSE(tracker_.shutdown_completed());
    EXPECT_TRUE(tracker_.IsShuttingDownForTesting());
  }

  TaskTracker tracker_;
  size_t num_tasks_executed_ = 0;
  std::queue<scoped_ptr<Task>> posted_tasks_;

 private:
  void PostTaskCallback(scoped_ptr<Task> task) {
    posted_tasks_.push(std::move(task));
  }

  void RunTaskCallback() { ++num_tasks_executed_; }

  scoped_ptr<ThreadCallingShutdown> thread_calling_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerTaskTrackerTest);
};

#define WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED() \
  do {                                      \
    SCOPED_TRACE("");                       \
    WaitForAsyncShutdownCompleted();        \
  } while (false)

#define VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS() \
  do {                                      \
    SCOPED_TRACE("");                       \
    VerifyAsyncShutdownInProgress();        \
  } while (false)

}  // namespace

TEST_P(TaskSchedulerTaskTrackerTest, PostAndRunBeforeShutdown) {
  scoped_ptr<Task> task_to_post(CreateTask(GetParam()));
  const Task* task_to_post_raw = task_to_post.get();

  // Post the task.
  EXPECT_TRUE(posted_tasks_.empty());
  PostTaskViaTracker(std::move(task_to_post));
  EXPECT_EQ(1U, posted_tasks_.size());
  EXPECT_EQ(task_to_post_raw, posted_tasks_.front().get());

  // Run the posted task.
  EXPECT_EQ(0U, num_tasks_executed_);
  RunNextPostedTaskViaTracker();
  EXPECT_EQ(1U, num_tasks_executed_);

  // Shutdown() shouldn't block.
  tracker_.Shutdown();
}

TEST_P(TaskSchedulerTaskTrackerTest, PostAndRunLongTaskBeforeShutdown) {
  // Post a task that will block until |event| is signaled.
  EXPECT_TRUE(posted_tasks_.empty());
  WaitableEvent event(false, false);
  PostTaskViaTracker(make_scoped_ptr(
      new Task(FROM_HERE, Bind(&WaitableEvent::Wait, Unretained(&event)),
               TaskTraits().WithShutdownBehavior(GetParam()))));
  EXPECT_EQ(1U, posted_tasks_.size());

  // Run the task asynchronouly.
  ThreadRunningTask thread_running_task(&tracker_, posted_tasks_.front().get());
  thread_running_task.Start();

  // Initiate shutdown while the task is running.
  CallShutdownAsync();

  if (GetParam() == TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    // Shutdown should complete even with a CONTINUE_ON_SHUTDOWN in progress.
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
  } else {
    // Shutdown should block with any non CONTINUE_ON_SHUTDOWN task in progress.
    VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();
  }

  // Unblock the task.
  event.Signal();
  thread_running_task.Join();

  // Shutdown should now complete for a non CONTINUE_ON_SHUTDOWN task.
  if (GetParam() != TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(TaskSchedulerTaskTrackerTest, PostBeforeShutdownRunDuringShutdown) {
  scoped_ptr<Task> task_to_post(CreateTask(GetParam()));
  const Task* task_to_post_raw = task_to_post.get();

  // Post the task.
  EXPECT_TRUE(posted_tasks_.empty());
  PostTaskViaTracker(std::move(task_to_post));
  EXPECT_EQ(1U, posted_tasks_.size());
  EXPECT_EQ(task_to_post_raw, posted_tasks_.front().get());

  // Post a BLOCK_SHUTDOWN task just to block shutdown.
  PostTaskViaTracker(CreateTask(TaskShutdownBehavior::BLOCK_SHUTDOWN));

  // Call Shutdown() asynchronously.
  CallShutdownAsync();
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Try to run the first posted task. Only BLOCK_SHUTDOWN tasks should run,
  // others should be discarded.
  EXPECT_EQ(0U, num_tasks_executed_);
  RunNextPostedTaskViaTracker();
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 1U : 0U,
            num_tasks_executed_);
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  // Unblock shutdown by running the remaining BLOCK_SHUTDOWN task.
  RunNextPostedTaskViaTracker();
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 2U : 1U,
            num_tasks_executed_);
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(TaskSchedulerTaskTrackerTest, PostBeforeShutdownRunAfterShutdown) {
  scoped_ptr<Task> task_to_post(CreateTask(GetParam()));
  const Task* task_to_post_raw = task_to_post.get();

  // Post the task.
  EXPECT_TRUE(posted_tasks_.empty());
  PostTaskViaTracker(std::move(task_to_post));
  EXPECT_EQ(1U, posted_tasks_.size());
  EXPECT_EQ(task_to_post_raw, posted_tasks_.front().get());

  // Call Shutdown() asynchronously.
  CallShutdownAsync();
  EXPECT_EQ(0U, num_tasks_executed_);

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

    // Run the task to unblock shutdown.
    RunNextPostedTaskViaTracker();
    EXPECT_EQ(1U, num_tasks_executed_);
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

    // It is not possible to test running a BLOCK_SHUTDOWN task posted before
    // shutdown after shutdown because Shutdown() won't return if there are
    // pending BLOCK_SHUTDOWN tasks.
  } else {
    WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();

    // The task shouldn't be allowed to run after shutdown.
    RunNextPostedTaskViaTracker();
    EXPECT_EQ(0U, num_tasks_executed_);
  }
}

TEST_P(TaskSchedulerTaskTrackerTest, PostAndRunDuringShutdown) {
  // Post a BLOCK_SHUTDOWN task just to block shutdown.
  PostTaskViaTracker(CreateTask(TaskShutdownBehavior::BLOCK_SHUTDOWN));
  scoped_ptr<Task> block_shutdown_task = std::move(posted_tasks_.front());
  posted_tasks_.pop();

  // Call Shutdown() asynchronously.
  CallShutdownAsync();
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    // Post a BLOCK_SHUTDOWN task. This should succeed.
    EXPECT_TRUE(posted_tasks_.empty());
    PostTaskViaTracker(CreateTask(GetParam()));
    EXPECT_EQ(1U, posted_tasks_.size());

    // Run the BLOCK_SHUTDOWN task. This should succeed.
    EXPECT_EQ(0U, num_tasks_executed_);
    RunNextPostedTaskViaTracker();
    EXPECT_EQ(1U, num_tasks_executed_);
  } else {
    // It shouldn't be possible to post a non BLOCK_SHUTDOWN task.
    PostTaskViaTracker(CreateTask(GetParam()));
    EXPECT_TRUE(posted_tasks_.empty());

    // Don't try to run the task, because it hasn't been posted successfully.
  }

  // Unblock shutdown by running the BLOCK_SHUTDOWN task posted at the beginning
  // of the test.
  VERIFY_ASYNC_SHUTDOWN_IN_PROGRESS();
  tracker_.RunTask(block_shutdown_task.get());
  EXPECT_EQ(GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN ? 2U : 1U,
            num_tasks_executed_);
  WAIT_FOR_ASYNC_SHUTDOWN_COMPLETED();
}

TEST_P(TaskSchedulerTaskTrackerTest, PostAfterShutdown) {
  // It is not possible to post a task after shutdown.
  tracker_.Shutdown();
  EXPECT_TRUE(posted_tasks_.empty());

  if (GetParam() == TaskShutdownBehavior::BLOCK_SHUTDOWN) {
    EXPECT_DCHECK_DEATH({ PostTaskViaTracker(CreateTask(GetParam())); }, "");
  } else {
    PostTaskViaTracker(CreateTask(GetParam()));
  }

  EXPECT_TRUE(posted_tasks_.empty());
}

INSTANTIATE_TEST_CASE_P(
    ContinueOnShutdown,
    TaskSchedulerTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
INSTANTIATE_TEST_CASE_P(
    SkipOnShutdown,
    TaskSchedulerTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::SKIP_ON_SHUTDOWN));
INSTANTIATE_TEST_CASE_P(
    BlockShutdown,
    TaskSchedulerTaskTrackerTest,
    ::testing::Values(TaskShutdownBehavior::BLOCK_SHUTDOWN));

}  // namespace internal
}  // namespace base
