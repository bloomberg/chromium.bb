// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_thread_pool.h"

#include <stddef.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/sequence_sort_key.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

const size_t kNumThreadsInThreadPool = 4;
const size_t kNumThreadsPostingTasks = 4;
const size_t kNumTasksPostedPerThread = 150;

class TaskSchedulerThreadPoolTest
    : public testing::TestWithParam<ExecutionMode> {
 protected:
  TaskSchedulerThreadPoolTest() : delayed_task_manager_(Bind(&DoNothing)) {}

  void SetUp() override {
    thread_pool_ = SchedulerThreadPool::CreateThreadPool(
        ThreadPriority::NORMAL, kNumThreadsInThreadPool,
        Bind(&TaskSchedulerThreadPoolTest::EnqueueSequenceCallback,
             Unretained(this)),
        &task_tracker_, &delayed_task_manager_);
    ASSERT_TRUE(thread_pool_);
  }

  void TearDown() override {
    thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
    thread_pool_->JoinForTesting();
  }

  std::unique_ptr<SchedulerThreadPool> thread_pool_;

 private:
  void EnqueueSequenceCallback(scoped_refptr<Sequence> sequence) {
    // In production code, this callback would be implemented by the
    // TaskScheduler which would first determine which PriorityQueue the
    // sequence must be reinserted.
    const SequenceSortKey sort_key(sequence->GetSortKey());
    thread_pool_->EnqueueSequence(std::move(sequence), sort_key);
  }

  TaskTracker task_tracker_;
  DelayedTaskManager delayed_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerThreadPoolTest);
};

class TaskFactory {
 public:
  // Constructs a TaskFactory that posts tasks with |execution_mode| to
  // |thread_pool|.
  TaskFactory(SchedulerThreadPool* thread_pool, ExecutionMode execution_mode)
      : cv_(&lock_),
        task_runner_(thread_pool->CreateTaskRunnerWithTraits(TaskTraits(),
                                                             execution_mode)),
        execution_mode_(execution_mode) {}

  // Posts a task through |task_runner_|. If |post_nested_task| is true, the
  // task will post a new task when it runs. If |event| is set, the task will
  // block until it is signaled.
  void PostTestTask(bool post_nested_task, WaitableEvent* event) {
    AutoLock auto_lock(lock_);
    EXPECT_TRUE(task_runner_->PostTask(
        FROM_HERE,
        Bind(&TaskFactory::RunTaskCallback, Unretained(this),
             num_created_tasks_++, post_nested_task, Unretained(event))));
  }

  // Waits for all tasks posted by PostTestTask() to start running. It is not
  // guaranteed that the tasks have completed their execution when this returns.
  void WaitForAllTasksToRun() const {
    AutoLock auto_lock(lock_);
    while (ran_tasks_.size() < num_created_tasks_)
      cv_.Wait();
  }

  size_t NumRunTasks() const {
    AutoLock auto_lock(lock_);
    return ran_tasks_.size();
  }

  const TaskRunner* task_runner() const { return task_runner_.get(); }

 private:
  void RunTaskCallback(size_t task_index,
                       bool post_nested_task,
                       WaitableEvent* event) {
    if (post_nested_task)
      PostTestTask(false, nullptr);

    EXPECT_TRUE(task_runner_->RunsTasksOnCurrentThread());

    {
      AutoLock auto_lock(lock_);

      if (execution_mode_ == ExecutionMode::SEQUENCED &&
          task_index != ran_tasks_.size()) {
        ADD_FAILURE() << "A SEQUENCED task didn't run in the expected order.";
      }

      if (ran_tasks_.find(task_index) != ran_tasks_.end())
        ADD_FAILURE() << "A task ran more than once.";
      ran_tasks_.insert(task_index);

      cv_.Signal();
    }

    if (event)
      event->Wait();
  }

  // Synchronizes access to all members below.
  mutable Lock lock_;

  // Condition variable signaled when a task runs.
  mutable ConditionVariable cv_;

  // Task runner through which this factory posts tasks.
  const scoped_refptr<TaskRunner> task_runner_;

  // Execution mode of |task_runner_|.
  const ExecutionMode execution_mode_;

  // Number of tasks posted by PostTestTask().
  size_t num_created_tasks_ = 0;

  // Indexes of tasks that ran.
  std::unordered_set<size_t> ran_tasks_;

  DISALLOW_COPY_AND_ASSIGN(TaskFactory);
};

class ThreadPostingTasks : public SimpleThread {
 public:
  // Constructs a thread that posts tasks to |thread_pool| through an
  // |execution_mode| task runner. If |wait_for_all_threads_idle| is true, the
  // thread wait until all worker threads in |thread_pool| are idle before
  // posting a new task. If |post_nested_task| is true, each task posted by this
  // thread posts another task when it runs.
  ThreadPostingTasks(SchedulerThreadPool* thread_pool,
                     ExecutionMode execution_mode,
                     bool wait_for_all_threads_idle,
                     bool post_nested_task)
      : SimpleThread("ThreadPostingTasks"),
        thread_pool_(thread_pool),
        wait_for_all_threads_idle_(wait_for_all_threads_idle),
        post_nested_task_(post_nested_task),
        factory_(thread_pool_, execution_mode) {
    DCHECK(thread_pool_);
  }

  const TaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksOnCurrentThread());

    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i) {
      if (wait_for_all_threads_idle_)
        thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
      factory_.PostTestTask(post_nested_task_, nullptr);
    }
  }

  SchedulerThreadPool* const thread_pool_;
  const scoped_refptr<TaskRunner> task_runner_;
  const bool wait_for_all_threads_idle_;
  const bool post_nested_task_;
  TaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasks);
};

TEST_P(TaskSchedulerThreadPoolTest, PostTasks) {
  // Create threads to post tasks.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    const bool kWaitForAllThreadIdle = false;
    const bool kPostNestedTasks = false;
    threads_posting_tasks.push_back(WrapUnique(
        new ThreadPostingTasks(thread_pool_.get(), GetParam(),
                               kWaitForAllThreadIdle, kPostNestedTasks)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
    EXPECT_EQ(kNumTasksPostedPerThread,
              thread_posting_tasks->factory()->NumRunTasks());
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, PostTasksWaitAllThreadsIdle) {
  // Create threads to post tasks. To verify that worker threads can sleep and
  // be woken up when new tasks are posted, wait for all threads to become idle
  // before posting a new task.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    const bool kWaitForAllThreadIdle = true;
    const bool kPostNestedTasks = false;
    threads_posting_tasks.push_back(WrapUnique(
        new ThreadPostingTasks(thread_pool_.get(), GetParam(),
                               kWaitForAllThreadIdle, kPostNestedTasks)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
    EXPECT_EQ(kNumTasksPostedPerThread,
              thread_posting_tasks->factory()->NumRunTasks());
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, NestedPostTasks) {
  // Create threads to post tasks. Each task posted by these threads will post
  // another task when it runs.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    const bool kWaitForAllThreadIdle = false;
    const bool kPostNestedTasks = true;
    threads_posting_tasks.push_back(WrapUnique(
        new ThreadPostingTasks(thread_pool_.get(), GetParam(),
                               kWaitForAllThreadIdle, kPostNestedTasks)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
    EXPECT_EQ(2 * kNumTasksPostedPerThread,
              thread_posting_tasks->factory()->NumRunTasks());
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, PostTasksWithOneAvailableThread) {
  // Post tasks to keep all threads busy except one until |event| is signaled.
  // Use different factories so that tasks are added to different sequences and
  // can run simultaneously when the execution mode is SEQUENCED.
  WaitableEvent event(true, false);
  std::vector<std::unique_ptr<TaskFactory>> blocked_task_factories;
  for (size_t i = 0; i < (kNumThreadsInThreadPool - 1); ++i) {
    blocked_task_factories.push_back(
        WrapUnique(new TaskFactory(thread_pool_.get(), GetParam())));
    blocked_task_factories.back()->PostTestTask(false, &event);
    blocked_task_factories.back()->WaitForAllTasksToRun();
  }

  // Post |kNumTasksPostedPerThread| tasks that should all run despite the fact
  // that only one thread in |thread_pool_| isn't busy.
  TaskFactory short_task_factory(thread_pool_.get(), GetParam());
  for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
    short_task_factory.PostTestTask(false, nullptr);
  short_task_factory.WaitForAllTasksToRun();

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TaskFactory after it is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, Saturate) {
  // Verify that it is possible to have |kNumThreadsInThreadPool|
  // tasks/sequences running simultaneously. Use different factories so that
  // tasks are added to different sequences and can run simultaneously when the
  // execution mode is SEQUENCED.
  WaitableEvent event(true, false);
  std::vector<std::unique_ptr<TaskFactory>> factories;
  for (size_t i = 0; i < kNumThreadsInThreadPool; ++i) {
    factories.push_back(
        WrapUnique(new TaskFactory(thread_pool_.get(), GetParam())));
    factories.back()->PostTestTask(false, &event);
    factories.back()->WaitForAllTasksToRun();
  }

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TaskFactory after it is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

INSTANTIATE_TEST_CASE_P(Parallel,
                        TaskSchedulerThreadPoolTest,
                        ::testing::Values(ExecutionMode::PARALLEL));
INSTANTIATE_TEST_CASE_P(Sequenced,
                        TaskSchedulerThreadPoolTest,
                        ::testing::Values(ExecutionMode::SEQUENCED));

}  // namespace
}  // namespace internal
}  // namespace base
