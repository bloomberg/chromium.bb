// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_worker_pool.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_scheduler/delayed_task_manager.h"
#include "base/task/task_scheduler/scheduler_sequenced_task_runner.h"
#include "base/task/task_scheduler/scheduler_worker_pool_impl.h"
#include "base/task/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/task/task_scheduler/test_task_factory.h"
#include "base/task/task_scheduler/test_utils.h"
#include "base/task/task_traits.h"
#include "base/task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/task/task_scheduler/platform_native_worker_pool_win.h"
#include "base/win/com_init_util.h"
#elif defined(OS_MACOSX)
#include "base/task/task_scheduler/platform_native_worker_pool_mac.h"
#endif

namespace base {
namespace internal {

namespace {

#if defined(OS_WIN) || defined(OS_MACOSX)
using PlatformNativeWorkerPoolType =
#if defined(OS_WIN)
    PlatformNativeWorkerPoolWin;
#elif defined(OS_MACOSX)
    PlatformNativeWorkerPoolMac;
#endif
#endif

constexpr size_t kMaxTasks = 4;
// By default, tests allow half of the pool to be used by best-effort tasks.
constexpr size_t kMaxBestEffortTasks = kMaxTasks / 2;
constexpr size_t kNumThreadsPostingTasks = 4;
constexpr size_t kNumTasksPostedPerThread = 150;

struct PoolExecutionType {
  test::PoolType pool_type;
  test::ExecutionMode execution_mode;
};

using PostNestedTask = test::TestTaskFactory::PostNestedTask;

class ThreadPostingTasks : public SimpleThread {
 public:
  // Constructs a thread that posts |num_tasks_posted_per_thread| tasks to
  // |worker_pool| through an |execution_mode| task runner. If
  // |post_nested_task| is YES, each task posted by this thread posts another
  // task when it runs.
  ThreadPostingTasks(test::MockSchedulerTaskRunnerDelegate*
                         mock_scheduler_task_runner_delegate_,
                     test::ExecutionMode execution_mode,
                     PostNestedTask post_nested_task)
      : SimpleThread("ThreadPostingTasks"),
        post_nested_task_(post_nested_task),
        factory_(test::CreateTaskRunnerWithExecutionMode(
                     execution_mode,
                     mock_scheduler_task_runner_delegate_),
                 execution_mode) {}

  const test::TestTaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksInCurrentSequence());

    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
      EXPECT_TRUE(factory_.PostTask(post_nested_task_, Closure()));
  }

  const scoped_refptr<TaskRunner> task_runner_;
  const PostNestedTask post_nested_task_;
  test::TestTaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasks);
};

class TaskSchedulerWorkerPoolTest
    : public testing::TestWithParam<PoolExecutionType>,
      public SchedulerWorkerPool::Delegate {
 protected:
  TaskSchedulerWorkerPoolTest()
      : service_thread_("TaskSchedulerServiceThread"),
        tracked_ref_factory_(this) {}

  void SetUp() override {
    service_thread_.Start();
    delayed_task_manager_.Start(service_thread_.task_runner());
    CreateWorkerPool();
  }

  void TearDown() override {
    service_thread_.Stop();
    if (worker_pool_)
      worker_pool_->JoinForTesting();
    worker_pool_.reset();
  }

  void CreateWorkerPool() {
    ASSERT_FALSE(worker_pool_);
    switch (GetParam().pool_type) {
      case test::PoolType::GENERIC:
        worker_pool_ = std::make_unique<SchedulerWorkerPoolImpl>(
            "TestWorkerPool", "A", ThreadPriority::NORMAL,
            task_tracker_.GetTrackedRef(),
            tracked_ref_factory_.GetTrackedRef());
        break;
#if defined(OS_WIN) || defined(OS_MACOSX)
      case test::PoolType::NATIVE:
        worker_pool_ = std::make_unique<PlatformNativeWorkerPoolType>(
            task_tracker_.GetTrackedRef(),
            tracked_ref_factory_.GetTrackedRef());
        break;
#endif
    }
    ASSERT_TRUE(worker_pool_);

    mock_scheduler_task_runner_delegate_.SetWorkerPool(worker_pool_.get());
  }

  void StartWorkerPool(
      SchedulerWorkerPool::WorkerEnvironment worker_environment =
          SchedulerWorkerPool::WorkerEnvironment::NONE) {
    ASSERT_TRUE(worker_pool_);
    switch (GetParam().pool_type) {
      case test::PoolType::GENERIC: {
        SchedulerWorkerPoolImpl* scheduler_worker_pool_impl =
            static_cast<SchedulerWorkerPoolImpl*>(worker_pool_.get());
        scheduler_worker_pool_impl->Start(
            SchedulerWorkerPoolParams(kMaxTasks, TimeDelta::Max()),
            kMaxBestEffortTasks, service_thread_.task_runner(), nullptr,
            worker_environment);
        break;
      }
#if defined(OS_WIN) || defined(OS_MACOSX)
      case test::PoolType::NATIVE: {
        PlatformNativeWorkerPoolType* scheduler_worker_pool_native_impl =
            static_cast<PlatformNativeWorkerPoolType*>(worker_pool_.get());
        scheduler_worker_pool_native_impl->Start(worker_environment);
        break;
      }
#endif
    }
  }

  Thread service_thread_;
  TaskTracker task_tracker_ = {"Test"};
  DelayedTaskManager delayed_task_manager_;
  test::MockSchedulerTaskRunnerDelegate mock_scheduler_task_runner_delegate_ = {
      task_tracker_.GetTrackedRef(), &delayed_task_manager_};

  std::unique_ptr<SchedulerWorkerPool> worker_pool_;

 private:
  // SchedulerWorkerPool::Delegate:
  SchedulerWorkerPool* GetWorkerPoolForTraits(
      const TaskTraits& traits) override {
    return worker_pool_.get();
  }

  TrackedRefFactory<SchedulerWorkerPool::Delegate> tracked_ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolTest);
};

void ShouldNotRun() {
  ADD_FAILURE() << "Ran a task that shouldn't run.";
}

}  // namespace

TEST_P(TaskSchedulerWorkerPoolTest, PostTasks) {
  StartWorkerPool();
  // Create threads to post tasks.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(std::make_unique<ThreadPostingTasks>(
        &mock_scheduler_task_runner_delegate_, GetParam().execution_mode,
        PostNestedTask::NO));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Flush the task tracker to be sure that no task accesses its TestTaskFactory
  // after |thread_posting_tasks| is destroyed.
  task_tracker_.FlushForTesting();
}

TEST_P(TaskSchedulerWorkerPoolTest, NestedPostTasks) {
  StartWorkerPool();
  // Create threads to post tasks. Each task posted by these threads will post
  // another task when it runs.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(std::make_unique<ThreadPostingTasks>(
        &mock_scheduler_task_runner_delegate_, GetParam().execution_mode,
        PostNestedTask::YES));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Flush the task tracker to be sure that no task accesses its TestTaskFactory
  // after |thread_posting_tasks| is destroyed.
  task_tracker_.FlushForTesting();
}

// Verify that a Task can't be posted after shutdown.
TEST_P(TaskSchedulerWorkerPoolTest, PostTaskAfterShutdown) {
  StartWorkerPool();
  auto task_runner = test::CreateTaskRunnerWithExecutionMode(
      GetParam().execution_mode, &mock_scheduler_task_runner_delegate_);
  task_tracker_.Shutdown();
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, BindOnce(&ShouldNotRun)));
}

// Verify that posting tasks after the pool was destroyed fails but doesn't
// crash.
TEST_P(TaskSchedulerWorkerPoolTest, PostAfterDestroy) {
  StartWorkerPool();
  auto task_runner = test::CreateTaskRunnerWithExecutionMode(
      GetParam().execution_mode, &mock_scheduler_task_runner_delegate_);
  EXPECT_TRUE(task_runner->PostTask(FROM_HERE, DoNothing()));
  task_tracker_.Shutdown();
  worker_pool_->JoinForTesting();
  worker_pool_.reset();
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, BindOnce(&ShouldNotRun)));
}

// Verify that a Task runs shortly after its delay expires.
TEST_P(TaskSchedulerWorkerPoolTest, PostDelayedTask) {
  StartWorkerPool();

  WaitableEvent task_ran(WaitableEvent::ResetPolicy::AUTOMATIC,
                         WaitableEvent::InitialState::NOT_SIGNALED);

  auto task_runner = test::CreateTaskRunnerWithExecutionMode(
      GetParam().execution_mode, &mock_scheduler_task_runner_delegate_);

  // Wait until the task runner is up and running to make sure the test below is
  // solely timing the delayed task, not bringing up a physical thread.
  task_runner->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
  ASSERT_TRUE(!task_ran.IsSignaled());

  // Post a task with a short delay.
  TimeTicks start_time = TimeTicks::Now();
  EXPECT_TRUE(task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)),
      TestTimeouts::tiny_timeout()));

  // Wait until the task runs.
  task_ran.Wait();

  // Expect the task to run after its delay expires, but no more than 250
  // ms after that.
  const TimeDelta actual_delay = TimeTicks::Now() - start_time;
  EXPECT_GE(actual_delay, TestTimeouts::tiny_timeout());
  EXPECT_LT(actual_delay,
            TimeDelta::FromMilliseconds(250) + TestTimeouts::tiny_timeout());
}

// Verify that the RunsTasksInCurrentSequence() method of a SEQUENCED TaskRunner
// returns false when called from a task that isn't part of the sequence. Note:
// Tests that use TestTaskFactory already verify that
// RunsTasksInCurrentSequence() returns true when appropriate so this method
// complements it to get full coverage of that method.
TEST_P(TaskSchedulerWorkerPoolTest, SequencedRunsTasksInCurrentSequence) {
  StartWorkerPool();
  auto task_runner = test::CreateTaskRunnerWithExecutionMode(
      GetParam().execution_mode, &mock_scheduler_task_runner_delegate_);
  auto sequenced_task_runner = test::CreateSequencedTaskRunnerWithTraits(
      TaskTraits(), &mock_scheduler_task_runner_delegate_);

  WaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<TaskRunner> sequenced_task_runner,
             WaitableEvent* task_ran) {
            EXPECT_FALSE(sequenced_task_runner->RunsTasksInCurrentSequence());
            task_ran->Signal();
          },
          sequenced_task_runner, Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that tasks posted before Start run after Start.
TEST_P(TaskSchedulerWorkerPoolTest, PostBeforeStart) {
  WaitableEvent task_1_running;
  WaitableEvent task_2_running;

  scoped_refptr<TaskRunner> task_runner = test::CreateTaskRunnerWithTraits(
      {WithBaseSyncPrimitives()}, &mock_scheduler_task_runner_delegate_);

  task_runner->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_1_running)));
  task_runner->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_2_running)));

  // Workers should not be created and tasks should not run before the pool is
  // started. The sleep is to give time for the tasks to potentially run.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_1_running.IsSignaled());
  EXPECT_FALSE(task_2_running.IsSignaled());

  StartWorkerPool();

  // Tasks should run shortly after the pool is started.
  task_1_running.Wait();
  task_2_running.Wait();

  task_tracker_.FlushForTesting();
}

// Verify that the maximum number of BEST_EFFORT tasks that can run concurrently
// in a pool does not affect Sequences with a priority that was increased from
// BEST_EFFORT to USER_BLOCKING.
TEST_P(TaskSchedulerWorkerPoolTest, UpdatePriorityBestEffortToUserBlocking) {
  StartWorkerPool();

  SchedulerLock num_tasks_running_lock;
  std::unique_ptr<ConditionVariable> num_tasks_running_cv =
      num_tasks_running_lock.CreateConditionVariable();
  size_t num_tasks_running = 0;

  // Post |kMaxTasks| BEST_EFFORT tasks that block until they all start running.
  std::vector<scoped_refptr<SchedulerSequencedTaskRunner>> task_runners;

  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runners.push_back(MakeRefCounted<SchedulerSequencedTaskRunner>(
        TaskTraits(TaskPriority::BEST_EFFORT),
        &mock_scheduler_task_runner_delegate_));
    task_runners.back()->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          // Increment the number of tasks running.
          {
            AutoSchedulerLock auto_lock(num_tasks_running_lock);
            ++num_tasks_running;
          }
          num_tasks_running_cv->Broadcast();

          // Wait until all posted tasks are running.
          AutoSchedulerLock auto_lock(num_tasks_running_lock);
          while (num_tasks_running < kMaxTasks) {
            ScopedClearBlockingObserverForTesting clear_blocking_observer;
            ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
            num_tasks_running_cv->Wait();
          }
        }));
  }

  // Wait until |kMaxBestEffort| tasks start running.
  {
    AutoSchedulerLock auto_lock(num_tasks_running_lock);
    while (num_tasks_running < kMaxBestEffortTasks)
      num_tasks_running_cv->Wait();
  }

  // Update the priority of all TaskRunners to USER_BLOCKING.
  for (size_t i = 0; i < kMaxTasks; ++i)
    task_runners[i]->UpdatePriority(TaskPriority::USER_BLOCKING);

  // Wait until all posted tasks start running. This should not block forever,
  // even in a pool that enforces a maximum number of concurrent BEST_EFFORT
  // tasks lower than |kMaxTasks|.
  static_assert(kMaxBestEffortTasks < kMaxTasks, "");
  {
    AutoSchedulerLock auto_lock(num_tasks_running_lock);
    while (num_tasks_running < kMaxTasks)
      num_tasks_running_cv->Wait();
  }

  task_tracker_.FlushForTesting();
}

#if defined(OS_WIN)
TEST_P(TaskSchedulerWorkerPoolTest, COMMTAWorkerEnvironment) {
  StartWorkerPool(SchedulerWorkerPool::WorkerEnvironment::COM_MTA);
  auto task_runner = test::CreateTaskRunnerWithExecutionMode(
      GetParam().execution_mode, &mock_scheduler_task_runner_delegate_);

  WaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](WaitableEvent* task_ran) {
                       win::AssertComApartmentType(win::ComApartmentType::MTA);
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}

TEST_P(TaskSchedulerWorkerPoolTest, NoWorkerEnvironment) {
  StartWorkerPool(SchedulerWorkerPool::WorkerEnvironment::NONE);
  auto task_runner = test::CreateTaskRunnerWithExecutionMode(
      GetParam().execution_mode, &mock_scheduler_task_runner_delegate_);

  WaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](WaitableEvent* task_ran) {
                       win::AssertComApartmentType(win::ComApartmentType::NONE);
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}
#endif

INSTANTIATE_TEST_SUITE_P(GenericParallel,
                         TaskSchedulerWorkerPoolTest,
                         ::testing::Values(PoolExecutionType{
                             test::PoolType::GENERIC,
                             test::ExecutionMode::PARALLEL}));
INSTANTIATE_TEST_SUITE_P(GenericSequenced,
                         TaskSchedulerWorkerPoolTest,
                         ::testing::Values(PoolExecutionType{
                             test::PoolType::GENERIC,
                             test::ExecutionMode::SEQUENCED}));

#if defined(OS_WIN) || defined(OS_MACOSX)
INSTANTIATE_TEST_SUITE_P(NativeParallel,
                         TaskSchedulerWorkerPoolTest,
                         ::testing::Values(PoolExecutionType{
                             test::PoolType::NATIVE,
                             test::ExecutionMode::PARALLEL}));
INSTANTIATE_TEST_SUITE_P(NativeSequenced,
                         TaskSchedulerWorkerPoolTest,
                         ::testing::Values(PoolExecutionType{
                             test::PoolType::NATIVE,
                             test::ExecutionMode::SEQUENCED}));
#endif

}  // namespace internal
}  // namespace base
