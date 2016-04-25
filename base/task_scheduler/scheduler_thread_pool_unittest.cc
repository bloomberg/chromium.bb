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
#include "base/task_scheduler/test_task_factory.h"
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

using PostNestedTask = test::TestTaskFactory::PostNestedTask;

class ThreadPostingTasks : public SimpleThread {
 public:
  enum class WaitBeforePostTask {
    NO_WAIT,
    WAIT_FOR_ALL_THREADS_IDLE,
  };

  // Constructs a thread that posts tasks to |thread_pool| through an
  // |execution_mode| task runner. If |wait_before_post_task| is
  // WAIT_FOR_ALL_THREADS_IDLE, the thread waits until all worker threads in
  // |thread_pool| are idle before posting a new task. If |post_nested_task| is
  // YES, each task posted by this thread posts another task when it runs.
  ThreadPostingTasks(SchedulerThreadPool* thread_pool,
                     ExecutionMode execution_mode,
                     WaitBeforePostTask wait_before_post_task,
                     PostNestedTask post_nested_task)
      : SimpleThread("ThreadPostingTasks"),
        thread_pool_(thread_pool),
        wait_before_post_task_(wait_before_post_task),
        post_nested_task_(post_nested_task),
        factory_(thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(),
                                                          execution_mode),
                 execution_mode) {
    DCHECK(thread_pool_);
  }

  const test::TestTaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksOnCurrentThread());

    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i) {
      if (wait_before_post_task_ ==
          WaitBeforePostTask::WAIT_FOR_ALL_THREADS_IDLE) {
        thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
      }
      EXPECT_TRUE(factory_.PostTask(post_nested_task_, nullptr));
    }
  }

  SchedulerThreadPool* const thread_pool_;
  const scoped_refptr<TaskRunner> task_runner_;
  const WaitBeforePostTask wait_before_post_task_;
  const PostNestedTask post_nested_task_;
  test::TestTaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasks);
};

using WaitBeforePostTask = ThreadPostingTasks::WaitBeforePostTask;

TEST_P(TaskSchedulerThreadPoolTest, PostTasks) {
  // Create threads to post tasks.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(WrapUnique(new ThreadPostingTasks(
        thread_pool_.get(), GetParam(), WaitBeforePostTask::NO_WAIT,
        PostNestedTask::NO)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, PostTasksWaitAllThreadsIdle) {
  // Create threads to post tasks. To verify that worker threads can sleep and
  // be woken up when new tasks are posted, wait for all threads to become idle
  // before posting a new task.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(WrapUnique(new ThreadPostingTasks(
        thread_pool_.get(), GetParam(),
        WaitBeforePostTask::WAIT_FOR_ALL_THREADS_IDLE, PostNestedTask::NO)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, NestedPostTasks) {
  // Create threads to post tasks. Each task posted by these threads will post
  // another task when it runs.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(WrapUnique(new ThreadPostingTasks(
        thread_pool_.get(), GetParam(), WaitBeforePostTask::NO_WAIT,
        PostNestedTask::YES)));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, PostTasksWithOneAvailableThread) {
  // Post tasks to keep all threads busy except one until |event| is signaled.
  // Use different factories so that tasks are added to different sequences and
  // can run simultaneously when the execution mode is SEQUENCED.
  WaitableEvent event(true, false);
  std::vector<std::unique_ptr<test::TestTaskFactory>> blocked_task_factories;
  for (size_t i = 0; i < (kNumThreadsInThreadPool - 1); ++i) {
    blocked_task_factories.push_back(WrapUnique(new test::TestTaskFactory(
        thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam()),
        GetParam())));
    EXPECT_TRUE(
        blocked_task_factories.back()->PostTask(PostNestedTask::NO, &event));
    blocked_task_factories.back()->WaitForAllTasksToRun();
  }

  // Post |kNumTasksPostedPerThread| tasks that should all run despite the fact
  // that only one thread in |thread_pool_| isn't busy.
  test::TestTaskFactory short_task_factory(
      thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam()),
      GetParam());
  for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
    EXPECT_TRUE(short_task_factory.PostTask(PostNestedTask::NO, nullptr));
  short_task_factory.WaitForAllTasksToRun();

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  thread_pool_->WaitForAllWorkerThreadsIdleForTesting();
}

TEST_P(TaskSchedulerThreadPoolTest, Saturate) {
  // Verify that it is possible to have |kNumThreadsInThreadPool|
  // tasks/sequences running simultaneously. Use different factories so that
  // tasks are added to different sequences and can run simultaneously when the
  // execution mode is SEQUENCED.
  WaitableEvent event(true, false);
  std::vector<std::unique_ptr<test::TestTaskFactory>> factories;
  for (size_t i = 0; i < kNumThreadsInThreadPool; ++i) {
    factories.push_back(WrapUnique(new test::TestTaskFactory(
        thread_pool_->CreateTaskRunnerWithTraits(TaskTraits(), GetParam()),
        GetParam())));
    EXPECT_TRUE(factories.back()->PostTask(PostNestedTask::NO, &event));
    factories.back()->WaitForAllTasksToRun();
  }

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all worker threads are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
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
