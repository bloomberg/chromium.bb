// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_worker_pool_impl.h"

#include <stddef.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/atomicops.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/sequence_sort_key.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/task_scheduler/test_task_factory.h"
#include "base/task_scheduler/test_utils.h"
#include "base/test/gtest_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker_impl.h"
#include "base/threading/thread_local_storage.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/com_init_util.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {
namespace {

constexpr size_t kNumWorkersInWorkerPool = 4;
constexpr size_t kNumThreadsPostingTasks = 4;
constexpr size_t kNumTasksPostedPerThread = 150;
// This can't be lower because Windows' WaitableEvent wakes up too early when a
// small timeout is used. This results in many spurious wake ups before a worker
// is allowed to cleanup.
constexpr TimeDelta kReclaimTimeForCleanupTests =
    TimeDelta::FromMilliseconds(500);

// Waits on |event| in a scope where the blocking observer is null, to avoid
// affecting the worker capacity.
void WaitWithoutBlockingObserver(WaitableEvent* event) {
  internal::ScopedClearBlockingObserverForTesting clear_blocking_observer;
  event->Wait();
}

class TaskSchedulerWorkerPoolImplTestBase {
 protected:
  TaskSchedulerWorkerPoolImplTestBase()
      : service_thread_("TaskSchedulerServiceThread"){};

  void CommonSetUp() {
    CreateAndStartWorkerPool(TimeDelta::Max(), kNumWorkersInWorkerPool);
  }

  void CommonTearDown() {
    service_thread_.Stop();
    task_tracker_.FlushForTesting();
    worker_pool_->WaitForAllWorkersIdleForTesting();
    worker_pool_->JoinForTesting();
  }

  void CreateWorkerPool() {
    ASSERT_FALSE(worker_pool_);
    service_thread_.Start();
    delayed_task_manager_.Start(service_thread_.task_runner());
    worker_pool_ = std::make_unique<SchedulerWorkerPoolImpl>(
        "TestWorkerPool", "A", ThreadPriority::NORMAL,
        task_tracker_.GetTrackedRef(), &delayed_task_manager_);
    ASSERT_TRUE(worker_pool_);
  }

  virtual void StartWorkerPool(TimeDelta suggested_reclaim_time,
                               size_t num_workers) {
    ASSERT_TRUE(worker_pool_);
    worker_pool_->Start(
        SchedulerWorkerPoolParams(num_workers, suggested_reclaim_time),
        service_thread_.task_runner(), nullptr,
        SchedulerWorkerPoolImpl::WorkerEnvironment::NONE);
  }

  void CreateAndStartWorkerPool(TimeDelta suggested_reclaim_time,
                                size_t num_workers) {
    CreateWorkerPool();
    StartWorkerPool(suggested_reclaim_time, num_workers);
  }

  Thread service_thread_;
  TaskTracker task_tracker_ = {"Test"};

  std::unique_ptr<SchedulerWorkerPoolImpl> worker_pool_;

 private:
  DelayedTaskManager delayed_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolImplTestBase);
};

class TaskSchedulerWorkerPoolImplTest
    : public TaskSchedulerWorkerPoolImplTestBase,
      public testing::Test {
 protected:
  TaskSchedulerWorkerPoolImplTest() = default;

  void SetUp() override { TaskSchedulerWorkerPoolImplTestBase::CommonSetUp(); }

  void TearDown() override {
    TaskSchedulerWorkerPoolImplTestBase::CommonTearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolImplTest);
};

class TaskSchedulerWorkerPoolImplTestParam
    : public TaskSchedulerWorkerPoolImplTestBase,
      public testing::TestWithParam<test::ExecutionMode> {
 protected:
  TaskSchedulerWorkerPoolImplTestParam() = default;

  void SetUp() override { TaskSchedulerWorkerPoolImplTestBase::CommonSetUp(); }

  void TearDown() override {
    TaskSchedulerWorkerPoolImplTestBase::CommonTearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolImplTestParam);
};

using PostNestedTask = test::TestTaskFactory::PostNestedTask;

class ThreadPostingTasksWaitIdle : public SimpleThread {
 public:
  // Constructs a thread that posts tasks to |worker_pool| through an
  // |execution_mode| task runner. The thread waits until all workers in
  // |worker_pool| are idle before posting a new task.
  ThreadPostingTasksWaitIdle(SchedulerWorkerPoolImpl* worker_pool,
                             test::ExecutionMode execution_mode)
      : SimpleThread("ThreadPostingTasksWaitIdle"),
        worker_pool_(worker_pool),
        factory_(CreateTaskRunnerWithExecutionMode(worker_pool, execution_mode),
                 execution_mode) {
    DCHECK(worker_pool_);
  }

  const test::TestTaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksInCurrentSequence());

    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i) {
      worker_pool_->WaitForAllWorkersIdleForTesting();
      EXPECT_TRUE(factory_.PostTask(PostNestedTask::NO, Closure()));
    }
  }

  SchedulerWorkerPoolImpl* const worker_pool_;
  const scoped_refptr<TaskRunner> task_runner_;
  test::TestTaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasksWaitIdle);
};

}  // namespace

TEST_P(TaskSchedulerWorkerPoolImplTestParam, PostTasksWaitAllWorkersIdle) {
  // Create threads to post tasks. To verify that workers can sleep and be woken
  // up when new tasks are posted, wait for all workers to become idle before
  // posting a new task.
  std::vector<std::unique_ptr<ThreadPostingTasksWaitIdle>>
      threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(
        std::make_unique<ThreadPostingTasksWaitIdle>(worker_pool_.get(),
                                                     GetParam()));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all workers are idle to be sure that no task accesses its
  // TestTaskFactory after |thread_posting_tasks| is destroyed.
  worker_pool_->WaitForAllWorkersIdleForTesting();
}

TEST_P(TaskSchedulerWorkerPoolImplTestParam, PostTasksWithOneAvailableWorker) {
  // Post blocking tasks to keep all workers busy except one until |event| is
  // signaled. Use different factories so that tasks are added to different
  // sequences and can run simultaneously when the execution mode is SEQUENCED.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<std::unique_ptr<test::TestTaskFactory>> blocked_task_factories;
  for (size_t i = 0; i < (kNumWorkersInWorkerPool - 1); ++i) {
    blocked_task_factories.push_back(std::make_unique<test::TestTaskFactory>(
        CreateTaskRunnerWithExecutionMode(worker_pool_.get(), GetParam()),
        GetParam()));
    EXPECT_TRUE(blocked_task_factories.back()->PostTask(
        PostNestedTask::NO,
        BindOnce(&WaitWithoutBlockingObserver, Unretained(&event))));
    blocked_task_factories.back()->WaitForAllTasksToRun();
  }

  // Post |kNumTasksPostedPerThread| tasks that should all run despite the fact
  // that only one worker in |worker_pool_| isn't busy.
  test::TestTaskFactory short_task_factory(
      CreateTaskRunnerWithExecutionMode(worker_pool_.get(), GetParam()),
      GetParam());
  for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
    EXPECT_TRUE(short_task_factory.PostTask(PostNestedTask::NO, Closure()));
  short_task_factory.WaitForAllTasksToRun();

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all workers are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  worker_pool_->WaitForAllWorkersIdleForTesting();
}

TEST_P(TaskSchedulerWorkerPoolImplTestParam, Saturate) {
  // Verify that it is possible to have |kNumWorkersInWorkerPool|
  // tasks/sequences running simultaneously. Use different factories so that the
  // blocking tasks are added to different sequences and can run simultaneously
  // when the execution mode is SEQUENCED.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<std::unique_ptr<test::TestTaskFactory>> factories;
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    factories.push_back(std::make_unique<test::TestTaskFactory>(
        CreateTaskRunnerWithExecutionMode(worker_pool_.get(), GetParam()),
        GetParam()));
    EXPECT_TRUE(factories.back()->PostTask(
        PostNestedTask::NO,
        BindOnce(&WaitWithoutBlockingObserver, Unretained(&event))));
    factories.back()->WaitForAllTasksToRun();
  }

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all workers are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  worker_pool_->WaitForAllWorkersIdleForTesting();
}

#if defined(OS_WIN)
TEST_P(TaskSchedulerWorkerPoolImplTestParam, NoEnvironment) {
  // Verify that COM is not initialized in a SchedulerWorkerPoolImpl initialized
  // with SchedulerWorkerPoolImpl::WorkerEnvironment::NONE.
  scoped_refptr<TaskRunner> task_runner =
      CreateTaskRunnerWithExecutionMode(worker_pool_.get(), GetParam());

  WaitableEvent task_running(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](WaitableEvent* task_running) {
                       win::AssertComApartmentType(win::ComApartmentType::NONE);
                       task_running->Signal();
                     },
                     &task_running));

  task_running.Wait();

  worker_pool_->WaitForAllWorkersIdleForTesting();
}
#endif  // defined(OS_WIN)

INSTANTIATE_TEST_CASE_P(Parallel,
                        TaskSchedulerWorkerPoolImplTestParam,
                        ::testing::Values(test::ExecutionMode::PARALLEL));
INSTANTIATE_TEST_CASE_P(Sequenced,
                        TaskSchedulerWorkerPoolImplTestParam,
                        ::testing::Values(test::ExecutionMode::SEQUENCED));

#if defined(OS_WIN)

namespace {

class TaskSchedulerWorkerPoolImplTestCOMMTAParam
    : public TaskSchedulerWorkerPoolImplTestBase,
      public testing::TestWithParam<test::ExecutionMode> {
 protected:
  TaskSchedulerWorkerPoolImplTestCOMMTAParam() = default;

  void SetUp() override { TaskSchedulerWorkerPoolImplTestBase::CommonSetUp(); }

  void TearDown() override {
    TaskSchedulerWorkerPoolImplTestBase::CommonTearDown();
  }

 private:
  void StartWorkerPool(TimeDelta suggested_reclaim_time,
                       size_t num_workers) override {
    ASSERT_TRUE(worker_pool_);
    worker_pool_->Start(
        SchedulerWorkerPoolParams(num_workers, suggested_reclaim_time),
        service_thread_.task_runner(), nullptr,
        SchedulerWorkerPoolImpl::WorkerEnvironment::COM_MTA);
  }

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolImplTestCOMMTAParam);
};

}  // namespace

TEST_P(TaskSchedulerWorkerPoolImplTestCOMMTAParam, COMMTAInitialized) {
  // Verify that SchedulerWorkerPoolImpl workers have a COM MTA available.
  scoped_refptr<TaskRunner> task_runner =
      CreateTaskRunnerWithExecutionMode(worker_pool_.get(), GetParam());

  WaitableEvent task_running(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](WaitableEvent* task_running) {
                       win::AssertComApartmentType(win::ComApartmentType::MTA);
                       task_running->Signal();
                     },
                     &task_running));

  task_running.Wait();

  worker_pool_->WaitForAllWorkersIdleForTesting();
}

INSTANTIATE_TEST_CASE_P(Parallel,
                        TaskSchedulerWorkerPoolImplTestCOMMTAParam,
                        ::testing::Values(test::ExecutionMode::PARALLEL));
INSTANTIATE_TEST_CASE_P(Sequenced,
                        TaskSchedulerWorkerPoolImplTestCOMMTAParam,
                        ::testing::Values(test::ExecutionMode::SEQUENCED));

#endif  // defined(OS_WIN)

namespace {

class TaskSchedulerWorkerPoolImplPostTaskBeforeStartTest
    : public TaskSchedulerWorkerPoolImplTest {
 public:
  void SetUp() override {
    CreateWorkerPool();
    // Let the test start the worker pool.
  }
};

void TaskPostedBeforeStart(PlatformThreadRef* platform_thread_ref,
                           WaitableEvent* task_running,
                           WaitableEvent* barrier) {
  *platform_thread_ref = PlatformThread::CurrentRef();
  task_running->Signal();
  WaitWithoutBlockingObserver(barrier);
}

}  // namespace

// Verify that 2 tasks posted before Start() to a SchedulerWorkerPoolImpl with
// more than 2 workers run on different workers when Start() is called.
TEST_F(TaskSchedulerWorkerPoolImplPostTaskBeforeStartTest,
       PostTasksBeforeStart) {
  PlatformThreadRef task_1_thread_ref;
  PlatformThreadRef task_2_thread_ref;
  WaitableEvent task_1_running(WaitableEvent::ResetPolicy::MANUAL,
                               WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent task_2_running(WaitableEvent::ResetPolicy::MANUAL,
                               WaitableEvent::InitialState::NOT_SIGNALED);

  // This event is used to prevent a task from completing before the other task
  // starts running. If that happened, both tasks could run on the same worker
  // and this test couldn't verify that the correct number of workers were woken
  // up.
  WaitableEvent barrier(WaitableEvent::ResetPolicy::MANUAL,
                        WaitableEvent::InitialState::NOT_SIGNALED);

  worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()})
      ->PostTask(
          FROM_HERE,
          BindOnce(&TaskPostedBeforeStart, Unretained(&task_1_thread_ref),
                   Unretained(&task_1_running), Unretained(&barrier)));
  worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()})
      ->PostTask(
          FROM_HERE,
          BindOnce(&TaskPostedBeforeStart, Unretained(&task_2_thread_ref),
                   Unretained(&task_2_running), Unretained(&barrier)));

  // Workers should not be created and tasks should not run before the pool is
  // started.
  EXPECT_EQ(0U, worker_pool_->NumberOfWorkersForTesting());
  EXPECT_FALSE(task_1_running.IsSignaled());
  EXPECT_FALSE(task_2_running.IsSignaled());

  StartWorkerPool(TimeDelta::Max(), kNumWorkersInWorkerPool);

  // Tasks should run shortly after the pool is started.
  task_1_running.Wait();
  task_2_running.Wait();

  // Tasks should run on different threads.
  EXPECT_NE(task_1_thread_ref, task_2_thread_ref);

  barrier.Signal();
  task_tracker_.FlushForTesting();
}

// Verify that posting many tasks before Start will cause the number of workers
// to grow to |worker_capacity_| during Start.
TEST_F(TaskSchedulerWorkerPoolImplPostTaskBeforeStartTest, PostManyTasks) {
  scoped_refptr<TaskRunner> task_runner =
      worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});
  constexpr size_t kNumTasksPosted = 2 * kNumWorkersInWorkerPool;
  for (size_t i = 0; i < kNumTasksPosted; ++i)
    task_runner->PostTask(FROM_HERE, DoNothing());

  EXPECT_EQ(0U, worker_pool_->NumberOfWorkersForTesting());

  StartWorkerPool(TimeDelta::Max(), kNumWorkersInWorkerPool);
  ASSERT_GT(kNumTasksPosted, worker_pool_->GetWorkerCapacityForTesting());
  EXPECT_EQ(kNumWorkersInWorkerPool,
            worker_pool_->GetWorkerCapacityForTesting());

  EXPECT_EQ(worker_pool_->NumberOfWorkersForTesting(),
            worker_pool_->GetWorkerCapacityForTesting());
}

namespace {

constexpr size_t kMagicTlsValue = 42;

class TaskSchedulerWorkerPoolCheckTlsReuse
    : public TaskSchedulerWorkerPoolImplTest {
 public:
  void SetTlsValueAndWait() {
    slot_.Set(reinterpret_cast<void*>(kMagicTlsValue));
    WaitWithoutBlockingObserver(&waiter_);
  }

  void CountZeroTlsValuesAndWait(WaitableEvent* count_waiter) {
    if (!slot_.Get())
      subtle::NoBarrier_AtomicIncrement(&zero_tls_values_, 1);

    count_waiter->Signal();
    WaitWithoutBlockingObserver(&waiter_);
  }

 protected:
  TaskSchedulerWorkerPoolCheckTlsReuse() :
      waiter_(WaitableEvent::ResetPolicy::MANUAL,
              WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetUp() override {
    CreateAndStartWorkerPool(kReclaimTimeForCleanupTests,
                             kNumWorkersInWorkerPool);
  }

  subtle::Atomic32 zero_tls_values_ = 0;

  WaitableEvent waiter_;

 private:
  ThreadLocalStorage::Slot slot_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolCheckTlsReuse);
};

}  // namespace

// Checks that at least one worker has been cleaned up by checking the TLS.
TEST_F(TaskSchedulerWorkerPoolCheckTlsReuse, CheckCleanupWorkers) {
  // Saturate the workers and mark each worker's thread with a magic TLS value.
  std::vector<std::unique_ptr<test::TestTaskFactory>> factories;
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    factories.push_back(std::make_unique<test::TestTaskFactory>(
        worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()}),
        test::ExecutionMode::PARALLEL));
    ASSERT_TRUE(factories.back()->PostTask(
        PostNestedTask::NO,
        Bind(&TaskSchedulerWorkerPoolCheckTlsReuse::SetTlsValueAndWait,
             Unretained(this))));
    factories.back()->WaitForAllTasksToRun();
  }

  // Release tasks waiting on |waiter_|.
  waiter_.Signal();
  worker_pool_->WaitForAllWorkersIdleForTesting();

  // All workers should be done running by now, so reset for the next phase.
  waiter_.Reset();

  // Wait for the worker pool to clean up at least one worker.
  worker_pool_->WaitForWorkersCleanedUpForTesting(1U);

  // Saturate and count the worker threads that do not have the magic TLS value.
  // If the value is not there, that means we're at a new worker.
  std::vector<std::unique_ptr<WaitableEvent>> count_waiters;
  for (auto& factory : factories) {
    count_waiters.push_back(WrapUnique(new WaitableEvent(
        WaitableEvent::ResetPolicy::MANUAL,
        WaitableEvent::InitialState::NOT_SIGNALED)));
    ASSERT_TRUE(factory->PostTask(
          PostNestedTask::NO,
          Bind(&TaskSchedulerWorkerPoolCheckTlsReuse::CountZeroTlsValuesAndWait,
               Unretained(this),
               count_waiters.back().get())));
    factory->WaitForAllTasksToRun();
  }

  // Wait for all counters to complete.
  for (auto& count_waiter : count_waiters)
    count_waiter->Wait();

  EXPECT_GT(subtle::NoBarrier_Load(&zero_tls_values_), 0);

  // Release tasks waiting on |waiter_|.
  waiter_.Signal();
}

namespace {

class TaskSchedulerWorkerPoolHistogramTest
    : public TaskSchedulerWorkerPoolImplTest {
 public:
  TaskSchedulerWorkerPoolHistogramTest() = default;

 protected:
  // Override SetUp() to allow every test case to initialize a worker pool with
  // its own arguments.
  void SetUp() override {}

  // Floods |worker_pool_| with a single task each that blocks until
  // |continue_event| is signaled. Every worker in the pool is blocked on
  // |continue_event| when this method returns. Note: this helper can easily be
  // generalized to be useful in other tests, but it's here for now because it's
  // only used in a TaskSchedulerWorkerPoolHistogramTest at the moment.
  void FloodPool(WaitableEvent* continue_event) {
    ASSERT_FALSE(continue_event->IsSignaled());

    auto task_runner =
        worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});

    const auto pool_capacity = worker_pool_->GetWorkerCapacityForTesting();

    WaitableEvent workers_flooded(WaitableEvent::ResetPolicy::MANUAL,
                                  WaitableEvent::InitialState::NOT_SIGNALED);
    RepeatingClosure all_workers_running_barrier = BarrierClosure(
        pool_capacity,
        BindOnce(&WaitableEvent::Signal, Unretained(&workers_flooded)));
    for (size_t i = 0; i < pool_capacity; ++i) {
      task_runner->PostTask(
          FROM_HERE,
          BindOnce(
              [](OnceClosure on_running, WaitableEvent* continue_event) {
                std::move(on_running).Run();
                WaitWithoutBlockingObserver(continue_event);
              },
              all_workers_running_barrier, continue_event));
    }
    workers_flooded.Wait();
  }

 private:
  std::unique_ptr<StatisticsRecorder> statistics_recorder_ =
      StatisticsRecorder::CreateTemporaryForTesting();

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolHistogramTest);
};

}  // namespace

TEST_F(TaskSchedulerWorkerPoolHistogramTest, NumTasksBetweenWaits) {
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  CreateAndStartWorkerPool(TimeDelta::Max(), kNumWorkersInWorkerPool);
  auto task_runner = worker_pool_->CreateSequencedTaskRunnerWithTraits(
      {WithBaseSyncPrimitives()});

  // Post a task.
  task_runner->PostTask(
      FROM_HERE, BindOnce(&WaitWithoutBlockingObserver, Unretained(&event)));

  // Post 2 more tasks while the first task hasn't completed its execution. It
  // is guaranteed that these tasks will run immediately after the first task,
  // without allowing the worker to sleep.
  task_runner->PostTask(FROM_HERE, DoNothing());
  task_runner->PostTask(FROM_HERE, DoNothing());

  // Allow tasks to run and wait until the SchedulerWorker is idle.
  event.Signal();
  worker_pool_->WaitForAllWorkersIdleForTesting();

  // Wake up the SchedulerWorker that just became idle by posting a task and
  // wait until it becomes idle again. The SchedulerWorker should record the
  // TaskScheduler.NumTasksBetweenWaits.* histogram on wake up.
  task_runner->PostTask(FROM_HERE, DoNothing());
  worker_pool_->WaitForAllWorkersIdleForTesting();

  // Verify that counts were recorded to the histogram as expected.
  const auto* histogram = worker_pool_->num_tasks_between_waits_histogram();
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(0));
  EXPECT_EQ(1, histogram->SnapshotSamples()->GetCount(3));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(10));
}

// Verifies that NumTasksBetweenWaits histogram is logged as expected across
// idle and cleanup periods.
TEST_F(TaskSchedulerWorkerPoolHistogramTest,
       NumTasksBetweenWaitsWithIdlePeriodAndCleanup) {
  WaitableEvent tasks_can_exit_event(WaitableEvent::ResetPolicy::MANUAL,
                                     WaitableEvent::InitialState::NOT_SIGNALED);
  CreateAndStartWorkerPool(kReclaimTimeForCleanupTests,
                           kNumWorkersInWorkerPool);

  WaitableEvent workers_continue(WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED);

  FloodPool(&workers_continue);

  const auto* histogram = worker_pool_->num_tasks_between_waits_histogram();

  // NumTasksBetweenWaits shouldn't be logged until idle.
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(0));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(1));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(10));

  // Make all workers go idle.
  workers_continue.Signal();
  worker_pool_->WaitForAllWorkersIdleForTesting();

  // All workers should have reported a single hit in the "1" bucket per the the
  // histogram being reported when going idle and each worker having processed
  // precisely 1 task per the controlled flooding logic above.
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(0));
  EXPECT_EQ(static_cast<int>(kNumWorkersInWorkerPool),
            histogram->SnapshotSamples()->GetCount(1));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(10));

  worker_pool_->WaitForWorkersCleanedUpForTesting(kNumWorkersInWorkerPool - 1);

  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(0));
  EXPECT_EQ(static_cast<int>(kNumWorkersInWorkerPool),
            histogram->SnapshotSamples()->GetCount(1));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(10));

  // Flooding the pool once again (without letting any workers go idle)
  // shouldn't affect the counts either.

  workers_continue.Reset();
  FloodPool(&workers_continue);

  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(0));
  EXPECT_EQ(static_cast<int>(kNumWorkersInWorkerPool),
            histogram->SnapshotSamples()->GetCount(1));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(10));

  workers_continue.Signal();
  worker_pool_->WaitForAllWorkersIdleForTesting();
}

TEST_F(TaskSchedulerWorkerPoolHistogramTest, NumTasksBeforeCleanup) {
  CreateWorkerPool();
  auto histogrammed_thread_task_runner =
      worker_pool_->CreateSequencedTaskRunnerWithTraits(
          {WithBaseSyncPrimitives()});

  // Post 3 tasks and hold the thread for idle thread stack ordering.
  // This test assumes |histogrammed_thread_task_runner| gets assigned the same
  // thread for each of its tasks.
  PlatformThreadRef thread_ref;
  histogrammed_thread_task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](PlatformThreadRef* thread_ref) {
                       ASSERT_TRUE(thread_ref);
                       *thread_ref = PlatformThread::CurrentRef();
                     },
                     Unretained(&thread_ref)));
  histogrammed_thread_task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](PlatformThreadRef* thread_ref) {
                       ASSERT_FALSE(thread_ref->is_null());
                       EXPECT_EQ(*thread_ref, PlatformThread::CurrentRef());
                     },
                     Unretained(&thread_ref)));

  WaitableEvent cleanup_thread_running(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent cleanup_thread_continue(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  histogrammed_thread_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](PlatformThreadRef* thread_ref,
             WaitableEvent* cleanup_thread_running,
             WaitableEvent* cleanup_thread_continue) {
            ASSERT_FALSE(thread_ref->is_null());
            EXPECT_EQ(*thread_ref, PlatformThread::CurrentRef());
            cleanup_thread_running->Signal();
            WaitWithoutBlockingObserver(cleanup_thread_continue);
          },
          Unretained(&thread_ref), Unretained(&cleanup_thread_running),
          Unretained(&cleanup_thread_continue)));

  // Start the worker pool with 2 workers, to avoid depending on the scheduler's
  // logic to always keep one extra idle worker.
  //
  // The pool is started after the 3 initial tasks have been posted to ensure
  // that they are scheduled on the same worker. If the tasks could run as they
  // are posted, there would be a chance that:
  // 1. Worker #1:        Runs a tasks and empties the sequence, without adding
  //                      itself to the idle stack yet.
  // 2. Posting thread:   Posts another task to the now empty sequence. Wakes
  //                      up a new worker, since worker #1 isn't on the idle
  //                      stack yet.
  // 3: Worker #2:        Runs the tasks, violating the expectation that the 3
  //                      initial tasks run on the same worker.
  constexpr size_t kTwoWorkers = 2;
  StartWorkerPool(kReclaimTimeForCleanupTests, kTwoWorkers);

  // Wait until the 3rd task is scheduled.
  cleanup_thread_running.Wait();

  // To allow the SchedulerWorker associated with
  // |histogrammed_thread_task_runner| to cleanup, make sure it isn't on top of
  // the idle stack by waking up another SchedulerWorker via
  // |task_runner_for_top_idle|. |histogrammed_thread_task_runner| should
  // release and go idle first and then |task_runner_for_top_idle| should
  // release and go idle. This allows the SchedulerWorker associated with
  // |histogrammed_thread_task_runner| to cleanup.
  WaitableEvent top_idle_thread_running(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent top_idle_thread_continue(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner_for_top_idle =
      worker_pool_->CreateSequencedTaskRunnerWithTraits(
          {WithBaseSyncPrimitives()});
  task_runner_for_top_idle->PostTask(
      FROM_HERE, BindOnce(
                     [](PlatformThreadRef thread_ref,
                        WaitableEvent* top_idle_thread_running,
                        WaitableEvent* top_idle_thread_continue) {
                       ASSERT_FALSE(thread_ref.is_null());
                       EXPECT_NE(thread_ref, PlatformThread::CurrentRef())
                           << "Worker reused. Worker will not cleanup and the "
                              "histogram value will be wrong.";
                       top_idle_thread_running->Signal();
                       WaitWithoutBlockingObserver(top_idle_thread_continue);
                     },
                     thread_ref, Unretained(&top_idle_thread_running),
                     Unretained(&top_idle_thread_continue)));
  top_idle_thread_running.Wait();
  EXPECT_EQ(0U, worker_pool_->NumberOfIdleWorkersForTesting());
  cleanup_thread_continue.Signal();
  // Wait for the cleanup thread to also become idle.
  worker_pool_->WaitForWorkersIdleForTesting(1U);
  top_idle_thread_continue.Signal();
  // Allow the thread processing the |histogrammed_thread_task_runner| work to
  // cleanup.
  worker_pool_->WaitForWorkersCleanedUpForTesting(1U);

  // Verify that counts were recorded to the histogram as expected.
  const auto* histogram = worker_pool_->num_tasks_before_detach_histogram();
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(0));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(1));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(2));
  EXPECT_EQ(1, histogram->SnapshotSamples()->GetCount(3));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(4));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(5));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(6));
  EXPECT_EQ(0, histogram->SnapshotSamples()->GetCount(10));
}

TEST(TaskSchedulerWorkerPoolStandbyPolicyTest, InitOne) {
  TaskTracker task_tracker("Test");
  DelayedTaskManager delayed_task_manager;
  scoped_refptr<TaskRunner> service_thread_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  delayed_task_manager.Start(service_thread_task_runner);
  auto worker_pool = std::make_unique<SchedulerWorkerPoolImpl>(
      "OnePolicyWorkerPool", "A", ThreadPriority::NORMAL,
      task_tracker.GetTrackedRef(), &delayed_task_manager);
  worker_pool->Start(SchedulerWorkerPoolParams(8U, TimeDelta::Max()),
                     service_thread_task_runner, nullptr,
                     SchedulerWorkerPoolImpl::WorkerEnvironment::NONE);
  ASSERT_TRUE(worker_pool);
  EXPECT_EQ(1U, worker_pool->NumberOfWorkersForTesting());
  worker_pool->JoinForTesting();
}

// Verify the SchedulerWorkerPoolImpl keeps at least one idle standby thread,
// capacity permitting.
TEST(TaskSchedulerWorkerPoolStandbyPolicyTest, VerifyStandbyThread) {
  constexpr size_t kWorkerCapacity = 3;

  TaskTracker task_tracker("Test");
  DelayedTaskManager delayed_task_manager;
  scoped_refptr<TaskRunner> service_thread_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  delayed_task_manager.Start(service_thread_task_runner);
  auto worker_pool = std::make_unique<SchedulerWorkerPoolImpl>(
      "StandbyThreadWorkerPool", "A", ThreadPriority::NORMAL,
      task_tracker.GetTrackedRef(), &delayed_task_manager);
  worker_pool->Start(
      SchedulerWorkerPoolParams(kWorkerCapacity, kReclaimTimeForCleanupTests),
      service_thread_task_runner, nullptr,
      SchedulerWorkerPoolImpl::WorkerEnvironment::NONE);
  ASSERT_TRUE(worker_pool);
  EXPECT_EQ(1U, worker_pool->NumberOfWorkersForTesting());

  auto task_runner =
      worker_pool->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});

  WaitableEvent thread_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent thread_continue(WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::NOT_SIGNALED);

  RepeatingClosure closure = BindRepeating(
      [](WaitableEvent* thread_running, WaitableEvent* thread_continue) {
        thread_running->Signal();
        WaitWithoutBlockingObserver(thread_continue);
      },
      Unretained(&thread_running), Unretained(&thread_continue));

  // There should be one idle thread until we reach worker capacity
  for (size_t i = 0; i < kWorkerCapacity; ++i) {
    EXPECT_EQ(i + 1, worker_pool->NumberOfWorkersForTesting());
    task_runner->PostTask(FROM_HERE, closure);
    thread_running.Wait();
  }

  // There should not be an extra idle thread if it means going above capacity
  EXPECT_EQ(kWorkerCapacity, worker_pool->NumberOfWorkersForTesting());

  thread_continue.Signal();
  // Wait long enough for all but one worker to clean up.
  worker_pool->WaitForWorkersCleanedUpForTesting(kWorkerCapacity - 1);
  EXPECT_EQ(1U, worker_pool->NumberOfWorkersForTesting());
  // Give extra time for a worker to cleanup : none should as the pool is
  // expected to keep a worker ready regardless of how long it was idle for.
  PlatformThread::Sleep(kReclaimTimeForCleanupTests);
  EXPECT_EQ(1U, worker_pool->NumberOfWorkersForTesting());

  worker_pool->JoinForTesting();
}

namespace {

enum class OptionalBlockingType {
  NO_BLOCK,
  MAY_BLOCK,
  WILL_BLOCK,
};

struct NestedBlockingType {
  NestedBlockingType(BlockingType first_in,
                     OptionalBlockingType second_in,
                     BlockingType behaves_as_in)
      : first(first_in), second(second_in), behaves_as(behaves_as_in) {}

  BlockingType first;
  OptionalBlockingType second;
  BlockingType behaves_as;
};

class NestedScopedBlockingCall {
 public:
  NestedScopedBlockingCall(const NestedBlockingType& nested_blocking_type)
      : first_scoped_blocking_call_(nested_blocking_type.first),
        second_scoped_blocking_call_(
            nested_blocking_type.second == OptionalBlockingType::WILL_BLOCK
                ? std::make_unique<ScopedBlockingCall>(BlockingType::WILL_BLOCK)
                : (nested_blocking_type.second ==
                           OptionalBlockingType::MAY_BLOCK
                       ? std::make_unique<ScopedBlockingCall>(
                             BlockingType::MAY_BLOCK)
                       : nullptr)) {}

 private:
  ScopedBlockingCall first_scoped_blocking_call_;
  std::unique_ptr<ScopedBlockingCall> second_scoped_blocking_call_;

  DISALLOW_COPY_AND_ASSIGN(NestedScopedBlockingCall);
};

}  // namespace

class TaskSchedulerWorkerPoolBlockingTest
    : public TaskSchedulerWorkerPoolImplTestBase,
      public testing::TestWithParam<NestedBlockingType> {
 public:
  TaskSchedulerWorkerPoolBlockingTest()
      : blocking_thread_running_(WaitableEvent::ResetPolicy::AUTOMATIC,
                                 WaitableEvent::InitialState::NOT_SIGNALED),
        blocking_thread_continue_(WaitableEvent::ResetPolicy::MANUAL,
                                  WaitableEvent::InitialState::NOT_SIGNALED) {}

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<NestedBlockingType> param_info) {
    std::string str = param_info.param.first == BlockingType::MAY_BLOCK
                          ? "MAY_BLOCK"
                          : "WILL_BLOCK";
    if (param_info.param.second == OptionalBlockingType::MAY_BLOCK)
      str += "_MAY_BLOCK";
    else if (param_info.param.second == OptionalBlockingType::WILL_BLOCK)
      str += "_WILL_BLOCK";
    return str;
  }

  void SetUp() override {
    TaskSchedulerWorkerPoolImplTestBase::CommonSetUp();
    task_runner_ =
        worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});
  }

  void TearDown() override {
    TaskSchedulerWorkerPoolImplTestBase::CommonTearDown();
  }

 protected:
  // Saturates the worker pool with a task that first blocks, waits to be
  // unblocked, then exits.
  void SaturateWithBlockingTasks(
      const NestedBlockingType& nested_blocking_type) {
    RepeatingClosure blocking_thread_running_closure =
        BarrierClosure(kNumWorkersInWorkerPool,
                       BindOnce(&WaitableEvent::Signal,
                                Unretained(&blocking_thread_running_)));

    for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
      task_runner_->PostTask(
          FROM_HERE,
          BindOnce(
              [](Closure* blocking_thread_running_closure,
                 WaitableEvent* blocking_thread_continue_,
                 const NestedBlockingType& nested_blocking_type) {
                NestedScopedBlockingCall nested_scoped_blocking_call(
                    nested_blocking_type);
                blocking_thread_running_closure->Run();
                WaitWithoutBlockingObserver(blocking_thread_continue_);
              },
              Unretained(&blocking_thread_running_closure),
              Unretained(&blocking_thread_continue_), nested_blocking_type));
    }
    blocking_thread_running_.Wait();
  }

  // Returns how long we can expect a change to |worker_capacity_| to occur
  // after a task has become blocked.
  TimeDelta GetWorkerCapacityChangeSleepTime() {
    return std::max(SchedulerWorkerPoolImpl::kBlockedWorkersPollPeriod,
                    worker_pool_->MayBlockThreshold()) +
           TestTimeouts::tiny_timeout();
  }

  // Waits indefinitely, until |worker_pool_|'s worker capacity increases to
  // |expected_worker_capacity|.
  void ExpectWorkerCapacityIncreasesTo(size_t expected_worker_capacity) {
    size_t capacity = worker_pool_->GetWorkerCapacityForTesting();
    while (capacity != expected_worker_capacity) {
      PlatformThread::Sleep(GetWorkerCapacityChangeSleepTime());
      size_t new_capacity = worker_pool_->GetWorkerCapacityForTesting();
      ASSERT_GE(new_capacity, capacity);
      capacity = new_capacity;
    }
  }

  // Unblocks tasks posted by SaturateWithBlockingTasks().
  void UnblockTasks() { blocking_thread_continue_.Signal(); }

  scoped_refptr<TaskRunner> task_runner_;

 private:
  WaitableEvent blocking_thread_running_;
  WaitableEvent blocking_thread_continue_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerWorkerPoolBlockingTest);
};

// Verify that BlockingScopeEntered() causes worker capacity to increase and
// creates a worker if needed. Also verify that BlockingScopeExited() decreases
// worker capacity after an increase.
TEST_P(TaskSchedulerWorkerPoolBlockingTest, ThreadBlockedUnblocked) {
  ASSERT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);

  SaturateWithBlockingTasks(GetParam());
  if (GetParam().behaves_as == BlockingType::MAY_BLOCK)
    ExpectWorkerCapacityIncreasesTo(2 * kNumWorkersInWorkerPool);
  // A range of possible number of workers is accepted because of
  // crbug.com/757897.
  EXPECT_GE(worker_pool_->NumberOfWorkersForTesting(),
            kNumWorkersInWorkerPool + 1);
  EXPECT_LE(worker_pool_->NumberOfWorkersForTesting(),
            2 * kNumWorkersInWorkerPool);
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            2 * kNumWorkersInWorkerPool);

  UnblockTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);
}

// Verify that tasks posted in a saturated pool before a ScopedBlockingCall will
// execute after ScopedBlockingCall is instantiated.
TEST_P(TaskSchedulerWorkerPoolBlockingTest, PostBeforeBlocking) {
  WaitableEvent thread_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent thread_can_block(WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent thread_continue(WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::NOT_SIGNALED);

  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    task_runner_->PostTask(
        FROM_HERE,
        BindOnce(
            [](const NestedBlockingType& nested_blocking_type,
               WaitableEvent* thread_running, WaitableEvent* thread_can_block,
               WaitableEvent* thread_continue) {
              thread_running->Signal();
              WaitWithoutBlockingObserver(thread_can_block);

              NestedScopedBlockingCall nested_scoped_blocking_call(
                  nested_blocking_type);
              WaitWithoutBlockingObserver(thread_continue);
            },
            GetParam(), Unretained(&thread_running),
            Unretained(&thread_can_block), Unretained(&thread_continue)));
    thread_running.Wait();
  }

  // All workers should be occupied and the pool should be saturated. Workers
  // have not entered ScopedBlockingCall yet.
  EXPECT_EQ(worker_pool_->NumberOfWorkersForTesting(), kNumWorkersInWorkerPool);
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);

  WaitableEvent extra_thread_running(WaitableEvent::ResetPolicy::MANUAL,
                                     WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent extra_threads_continue(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure extra_threads_running_barrier = BarrierClosure(
      kNumWorkersInWorkerPool,
      BindOnce(&WaitableEvent::Signal, Unretained(&extra_thread_running)));
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    task_runner_->PostTask(FROM_HERE,
                           BindOnce(
                               [](Closure* extra_threads_running_barrier,
                                  WaitableEvent* extra_threads_continue) {
                                 extra_threads_running_barrier->Run();
                                 WaitWithoutBlockingObserver(
                                     extra_threads_continue);
                               },
                               Unretained(&extra_threads_running_barrier),
                               Unretained(&extra_threads_continue)));
  }

  // Allow tasks to enter ScopedBlockingCall. Workers should be created for the
  // tasks we just posted.
  thread_can_block.Signal();
  if (GetParam().behaves_as == BlockingType::MAY_BLOCK)
    ExpectWorkerCapacityIncreasesTo(2 * kNumWorkersInWorkerPool);

  // Should not block forever.
  extra_thread_running.Wait();
  EXPECT_EQ(worker_pool_->NumberOfWorkersForTesting(),
            2 * kNumWorkersInWorkerPool);
  extra_threads_continue.Signal();

  thread_continue.Signal();
  task_tracker_.FlushForTesting();
}
// Verify that workers become idle when the pool is over-capacity and that
// those workers do no work.
TEST_P(TaskSchedulerWorkerPoolBlockingTest, WorkersIdleWhenOverCapacity) {
  ASSERT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);

  SaturateWithBlockingTasks(GetParam());
  if (GetParam().behaves_as == BlockingType::MAY_BLOCK)
    ExpectWorkerCapacityIncreasesTo(2 * kNumWorkersInWorkerPool);
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            2 * kNumWorkersInWorkerPool);
  // A range of possible number of workers is accepted because of
  // crbug.com/757897.
  EXPECT_GE(worker_pool_->NumberOfWorkersForTesting(),
            kNumWorkersInWorkerPool + 1);
  EXPECT_LE(worker_pool_->NumberOfWorkersForTesting(),
            2 * kNumWorkersInWorkerPool);

  WaitableEvent thread_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                               WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent thread_continue(WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::NOT_SIGNALED);

  RepeatingClosure thread_running_barrier = BarrierClosure(
      kNumWorkersInWorkerPool,
      BindOnce(&WaitableEvent::Signal, Unretained(&thread_running)));
  // Posting these tasks should cause new workers to be created.
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    auto callback = BindOnce(
        [](Closure* thread_running_barrier, WaitableEvent* thread_continue) {
          thread_running_barrier->Run();
          WaitWithoutBlockingObserver(thread_continue);
        },
        Unretained(&thread_running_barrier), Unretained(&thread_continue));
    task_runner_->PostTask(FROM_HERE, std::move(callback));
  }
  thread_running.Wait();

  ASSERT_EQ(worker_pool_->NumberOfIdleWorkersForTesting(), 0U);
  EXPECT_EQ(worker_pool_->NumberOfWorkersForTesting(),
            2 * kNumWorkersInWorkerPool);

  AtomicFlag is_exiting;
  // These tasks should not get executed until after other tasks become
  // unblocked.
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    task_runner_->PostTask(FROM_HERE, BindOnce(
                                          [](AtomicFlag* is_exiting) {
                                            EXPECT_TRUE(is_exiting->IsSet());
                                          },
                                          Unretained(&is_exiting)));
  }

  // The original |kNumWorkersInWorkerPool| will finish their tasks after being
  // unblocked. There will be work in the work queue, but the pool should now
  // be over-capacity and workers will become idle.
  UnblockTasks();
  worker_pool_->WaitForWorkersIdleForTesting(kNumWorkersInWorkerPool);
  EXPECT_EQ(worker_pool_->NumberOfIdleWorkersForTesting(),
            kNumWorkersInWorkerPool);

  // Posting more tasks should not cause workers idle from the pool being over
  // capacity to begin doing work.
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    task_runner_->PostTask(FROM_HERE, BindOnce(
                                          [](AtomicFlag* is_exiting) {
                                            EXPECT_TRUE(is_exiting->IsSet());
                                          },
                                          Unretained(&is_exiting)));
  }

  // Give time for those idle workers to possibly do work (which should not
  // happen).
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  is_exiting.Set();
  // Unblocks the new workers.
  thread_continue.Signal();
  task_tracker_.FlushForTesting();
}

INSTANTIATE_TEST_CASE_P(
    ,
    TaskSchedulerWorkerPoolBlockingTest,
    ::testing::Values(NestedBlockingType(BlockingType::MAY_BLOCK,
                                         OptionalBlockingType::NO_BLOCK,
                                         BlockingType::MAY_BLOCK),
                      NestedBlockingType(BlockingType::WILL_BLOCK,
                                         OptionalBlockingType::NO_BLOCK,
                                         BlockingType::WILL_BLOCK),
                      NestedBlockingType(BlockingType::MAY_BLOCK,
                                         OptionalBlockingType::WILL_BLOCK,
                                         BlockingType::WILL_BLOCK),
                      NestedBlockingType(BlockingType::WILL_BLOCK,
                                         OptionalBlockingType::MAY_BLOCK,
                                         BlockingType::WILL_BLOCK)),
    TaskSchedulerWorkerPoolBlockingTest::ParamInfoToString);

// Verify that if a thread enters the scope of a MAY_BLOCK ScopedBlockingCall,
// but exits the scope before the MayBlockThreshold() is reached, that the
// worker capacity does not increase.
TEST_F(TaskSchedulerWorkerPoolBlockingTest, ThreadBlockUnblockPremature) {
  ASSERT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);

  TimeDelta worker_capacity_change_sleep = GetWorkerCapacityChangeSleepTime();
  worker_pool_->MaximizeMayBlockThresholdForTesting();

  SaturateWithBlockingTasks(NestedBlockingType(BlockingType::MAY_BLOCK,
                                               OptionalBlockingType::NO_BLOCK,
                                               BlockingType::MAY_BLOCK));
  PlatformThread::Sleep(worker_capacity_change_sleep);
  EXPECT_EQ(worker_pool_->NumberOfWorkersForTesting(), kNumWorkersInWorkerPool);
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);

  UnblockTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);
}

// Verify that if worker capacity is incremented because of a MAY_BLOCK
// ScopedBlockingCall, it isn't incremented again when there is a nested
// WILL_BLOCK ScopedBlockingCall.
TEST_F(TaskSchedulerWorkerPoolBlockingTest,
       MayBlockIncreaseCapacityNestedWillBlock) {
  ASSERT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);
  auto task_runner =
      worker_pool_->CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});
  WaitableEvent can_return(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);

  // Saturate the pool so that a MAY_BLOCK ScopedBlockingCall would increment
  // the worker capacity.
  for (size_t i = 0; i < kNumWorkersInWorkerPool - 1; ++i) {
    task_runner->PostTask(FROM_HERE, BindOnce(&WaitWithoutBlockingObserver,
                                              Unretained(&can_return)));
  }

  WaitableEvent can_instantiate_will_block(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent did_instantiate_will_block(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);

  // Post a task that instantiates a MAY_BLOCK ScopedBlockingCall.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](WaitableEvent* can_instantiate_will_block,
             WaitableEvent* did_instantiate_will_block,
             WaitableEvent* can_return) {
            ScopedBlockingCall may_block(BlockingType::MAY_BLOCK);
            WaitWithoutBlockingObserver(can_instantiate_will_block);
            ScopedBlockingCall will_block(BlockingType::WILL_BLOCK);
            did_instantiate_will_block->Signal();
            WaitWithoutBlockingObserver(can_return);
          },
          Unretained(&can_instantiate_will_block),
          Unretained(&did_instantiate_will_block), Unretained(&can_return)));

  // After a short delay, worker capacity should be incremented.
  ExpectWorkerCapacityIncreasesTo(kNumWorkersInWorkerPool + 1);

  // Wait until the task instantiates a WILL_BLOCK ScopedBlockingCall.
  can_instantiate_will_block.Signal();
  did_instantiate_will_block.Wait();

  // Worker capacity shouldn't be incremented again.
  EXPECT_EQ(kNumWorkersInWorkerPool + 1,
            worker_pool_->GetWorkerCapacityForTesting());

  // Tear down.
  can_return.Signal();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool);
}

// Verify that workers that become idle due to the pool being over capacity will
// eventually cleanup.
TEST(TaskSchedulerWorkerPoolOverWorkerCapacityTest, VerifyCleanup) {
  constexpr size_t kWorkerCapacity = 3;

  TaskTracker task_tracker("Test");
  DelayedTaskManager delayed_task_manager;
  scoped_refptr<TaskRunner> service_thread_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  delayed_task_manager.Start(service_thread_task_runner);
  SchedulerWorkerPoolImpl worker_pool(
      "OverWorkerCapacityTestWorkerPool", "A", ThreadPriority::NORMAL,
      task_tracker.GetTrackedRef(), &delayed_task_manager);
  worker_pool.Start(
      SchedulerWorkerPoolParams(kWorkerCapacity, kReclaimTimeForCleanupTests),
      service_thread_task_runner, nullptr,
      SchedulerWorkerPoolImpl::WorkerEnvironment::NONE);

  scoped_refptr<TaskRunner> task_runner =
      worker_pool.CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});

  WaitableEvent threads_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent threads_continue(WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kWorkerCapacity,
      BindOnce(&WaitableEvent::Signal, Unretained(&threads_running)));

  WaitableEvent blocked_call_continue(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);

  RepeatingClosure closure = BindRepeating(
      [](Closure* threads_running_barrier, WaitableEvent* threads_continue,
         WaitableEvent* blocked_call_continue) {
        threads_running_barrier->Run();
        {
          ScopedBlockingCall scoped_blocking_call(BlockingType::WILL_BLOCK);
          WaitWithoutBlockingObserver(blocked_call_continue);
        }
        WaitWithoutBlockingObserver(threads_continue);
      },
      Unretained(&threads_running_barrier), Unretained(&threads_continue),
      Unretained(&blocked_call_continue));

  for (size_t i = 0; i < kWorkerCapacity; ++i)
    task_runner->PostTask(FROM_HERE, closure);

  threads_running.Wait();

  WaitableEvent extra_threads_running(
      WaitableEvent::ResetPolicy::AUTOMATIC,
      WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent extra_threads_continue(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);

  RepeatingClosure extra_threads_running_barrier = BarrierClosure(
      kWorkerCapacity,
      BindOnce(&WaitableEvent::Signal, Unretained(&extra_threads_running)));
  // These tasks should run on the new threads from increasing worker capacity.
  for (size_t i = 0; i < kWorkerCapacity; ++i) {
    task_runner->PostTask(FROM_HERE,
                          BindOnce(
                              [](Closure* extra_threads_running_barrier,
                                 WaitableEvent* extra_threads_continue) {
                                extra_threads_running_barrier->Run();
                                WaitWithoutBlockingObserver(
                                    extra_threads_continue);
                              },
                              Unretained(&extra_threads_running_barrier),
                              Unretained(&extra_threads_continue)));
  }
  extra_threads_running.Wait();

  ASSERT_EQ(kWorkerCapacity * 2, worker_pool.NumberOfWorkersForTesting());
  EXPECT_EQ(kWorkerCapacity * 2, worker_pool.GetWorkerCapacityForTesting());
  blocked_call_continue.Signal();
  extra_threads_continue.Signal();

  // Periodically post tasks to ensure that posting tasks does not prevent
  // workers that are idle due to the pool being over capacity from cleaning up.
  for (int i = 0; i < 16; ++i) {
    task_runner->PostDelayedTask(FROM_HERE, DoNothing(),
                                 kReclaimTimeForCleanupTests * i * 0.5);
  }

  // Note: one worker above capacity will not get cleaned up since it's on the
  // top of the idle stack.
  worker_pool.WaitForWorkersCleanedUpForTesting(kWorkerCapacity - 1);
  EXPECT_EQ(kWorkerCapacity + 1, worker_pool.NumberOfWorkersForTesting());

  threads_continue.Signal();

  worker_pool.JoinForTesting();
}

// Verify that the maximum number of workers is 256 and that hitting the max
// leaves the pool in a valid state with regards to worker capacity.
TEST_F(TaskSchedulerWorkerPoolBlockingTest, MaximumWorkersTest) {
  constexpr size_t kMaxNumberOfWorkers = 256;
  constexpr size_t kNumExtraTasks = 10;

  WaitableEvent early_blocking_thread_running(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure early_threads_barrier_closure =
      BarrierClosure(kMaxNumberOfWorkers,
                     BindOnce(&WaitableEvent::Signal,
                              Unretained(&early_blocking_thread_running)));

  WaitableEvent early_threads_finished(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure early_threads_finished_barrier = BarrierClosure(
      kMaxNumberOfWorkers,
      BindOnce(&WaitableEvent::Signal, Unretained(&early_threads_finished)));

  WaitableEvent early_release_thread_continue(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);

  // Post ScopedBlockingCall tasks to hit the worker cap.
  for (size_t i = 0; i < kMaxNumberOfWorkers; ++i) {
    task_runner_->PostTask(FROM_HERE,
                           BindOnce(
                               [](Closure* early_threads_barrier_closure,
                                  WaitableEvent* early_release_thread_continue,
                                  Closure* early_threads_finished) {
                                 {
                                   ScopedBlockingCall scoped_blocking_call(
                                       BlockingType::WILL_BLOCK);
                                   early_threads_barrier_closure->Run();
                                   WaitWithoutBlockingObserver(
                                       early_release_thread_continue);
                                 }
                                 early_threads_finished->Run();
                               },
                               Unretained(&early_threads_barrier_closure),
                               Unretained(&early_release_thread_continue),
                               Unretained(&early_threads_finished_barrier)));
  }

  early_blocking_thread_running.Wait();
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool + kMaxNumberOfWorkers);

  WaitableEvent late_release_thread_contine(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEvent late_blocking_thread_running(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure late_threads_barrier_closure = BarrierClosure(
      kNumExtraTasks, BindOnce(&WaitableEvent::Signal,
                               Unretained(&late_blocking_thread_running)));

  // Posts additional tasks. Note: we should already have |kMaxNumberOfWorkers|
  // tasks running. These tasks should not be able to get executed yet as
  // the pool is already at its max worker cap.
  for (size_t i = 0; i < kNumExtraTasks; ++i) {
    task_runner_->PostTask(
        FROM_HERE,
        BindOnce(
            [](Closure* late_threads_barrier_closure,
               WaitableEvent* late_release_thread_contine) {
              ScopedBlockingCall scoped_blocking_call(BlockingType::WILL_BLOCK);
              late_threads_barrier_closure->Run();
              WaitWithoutBlockingObserver(late_release_thread_contine);
            },
            Unretained(&late_threads_barrier_closure),
            Unretained(&late_release_thread_contine)));
  }

  // Give time to see if we exceed the max number of workers.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_LE(worker_pool_->NumberOfWorkersForTesting(), kMaxNumberOfWorkers);

  early_release_thread_continue.Signal();
  early_threads_finished.Wait();
  late_blocking_thread_running.Wait();

  WaitableEvent final_tasks_running(WaitableEvent::ResetPolicy::MANUAL,
                                    WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent final_tasks_continue(WaitableEvent::ResetPolicy::MANUAL,
                                     WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure final_tasks_running_barrier = BarrierClosure(
      kNumWorkersInWorkerPool,
      BindOnce(&WaitableEvent::Signal, Unretained(&final_tasks_running)));

  // Verify that we are still able to saturate the pool.
  for (size_t i = 0; i < kNumWorkersInWorkerPool; ++i) {
    task_runner_->PostTask(
        FROM_HERE,
        BindOnce(
            [](Closure* closure, WaitableEvent* final_tasks_continue) {
              closure->Run();
              WaitWithoutBlockingObserver(final_tasks_continue);
            },
            Unretained(&final_tasks_running_barrier),
            Unretained(&final_tasks_continue)));
  }
  final_tasks_running.Wait();
  EXPECT_EQ(worker_pool_->GetWorkerCapacityForTesting(),
            kNumWorkersInWorkerPool + kNumExtraTasks);
  late_release_thread_contine.Signal();
  final_tasks_continue.Signal();
  task_tracker_.FlushForTesting();
}

// Verify that worker detachement doesn't race with worker cleanup, regression
// test for https://crbug.com/810464.
TEST(TaskSchedulerWorkerPoolTest, RacyCleanup) {
#if defined(OS_FUCHSIA)
  // Fuchsia + QEMU doesn't deal well with *many* threads being
  // created/destroyed at once: https://crbug.com/816575.
  constexpr size_t kWorkerCapacity = 16;
#else   // defined(OS_FUCHSIA)
  constexpr size_t kWorkerCapacity = 256;
#endif  // defined(OS_FUCHSIA)
  constexpr TimeDelta kReclaimTimeForRacyCleanupTest =
      TimeDelta::FromMilliseconds(10);

  TaskTracker task_tracker("Test");
  DelayedTaskManager delayed_task_manager;
  scoped_refptr<TaskRunner> service_thread_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  delayed_task_manager.Start(service_thread_task_runner);
  SchedulerWorkerPoolImpl worker_pool(
      "RacyCleanupTestWorkerPool", "A", ThreadPriority::NORMAL,
      task_tracker.GetTrackedRef(), &delayed_task_manager);
  worker_pool.Start(SchedulerWorkerPoolParams(kWorkerCapacity,
                                              kReclaimTimeForRacyCleanupTest),
                    service_thread_task_runner, nullptr,
                    SchedulerWorkerPoolImpl::WorkerEnvironment::NONE);

  scoped_refptr<TaskRunner> task_runner =
      worker_pool.CreateTaskRunnerWithTraits({WithBaseSyncPrimitives()});

  WaitableEvent threads_running(WaitableEvent::ResetPolicy::AUTOMATIC,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent unblock_threads(WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::NOT_SIGNALED);
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kWorkerCapacity,
      BindOnce(&WaitableEvent::Signal, Unretained(&threads_running)));

  for (size_t i = 0; i < kWorkerCapacity; ++i) {
    task_runner->PostTask(
        FROM_HERE,
        BindOnce(
            [](OnceClosure on_running, WaitableEvent* unblock_threads) {
              std::move(on_running).Run();
              WaitWithoutBlockingObserver(unblock_threads);
            },
            threads_running_barrier, Unretained(&unblock_threads)));
  }

  // Wait for all workers to be ready and release them all at once.
  threads_running.Wait();
  unblock_threads.Signal();

  // Sleep to wakeup precisely when all workers are going to try to cleanup per
  // being idle.
  PlatformThread::Sleep(kReclaimTimeForRacyCleanupTest);

  worker_pool.JoinForTesting();

  // Unwinding this test will be racy if worker cleanup can race with
  // SchedulerWorkerPoolImpl destruction : https://crbug.com/810464.
}

}  // namespace internal
}  // namespace base
