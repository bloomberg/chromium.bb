// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_TEST_UTILS_H_
#define BASE_TASK_THREAD_POOL_TEST_UTILS_H_

#include "base/task/common/checked_lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/delayed_task_manager.h"
#include "base/task/thread_pool/scheduler_task_runner_delegate.h"
#include "base/task/thread_pool/scheduler_worker_observer.h"
#include "base/task/thread_pool/scheduler_worker_pool.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task_runner.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace internal {

struct Task;

namespace test {

class MockSchedulerWorkerObserver : public SchedulerWorkerObserver {
 public:
  MockSchedulerWorkerObserver();
  ~MockSchedulerWorkerObserver();

  void AllowCallsOnMainExit(int num_calls);
  void WaitCallsOnMainExit();

  // SchedulerWorkerObserver:
  MOCK_METHOD0(OnSchedulerWorkerMainEntry, void());
  // This doesn't use MOCK_METHOD0 because some tests need to wait for all calls
  // to happen, which isn't possible with gmock.
  void OnSchedulerWorkerMainExit() override;

 private:
  CheckedLock lock_;
  std::unique_ptr<ConditionVariable> on_main_exit_cv_ GUARDED_BY(lock_);
  int allowed_calls_on_main_exit_ GUARDED_BY(lock_) = 0;

  DISALLOW_COPY_AND_ASSIGN(MockSchedulerWorkerObserver);
};

class MockSchedulerTaskRunnerDelegate : public SchedulerTaskRunnerDelegate {
 public:
  MockSchedulerTaskRunnerDelegate(TrackedRef<TaskTracker> task_tracker,
                                  DelayedTaskManager* delayed_task_manager);
  ~MockSchedulerTaskRunnerDelegate() override;

  // SchedulerTaskRunnerDelegate:
  bool PostTaskWithSequence(Task task,
                            scoped_refptr<Sequence> sequence) override;
  bool IsRunningPoolWithTraits(const TaskTraits& traits) const override;
  void UpdatePriority(scoped_refptr<TaskSource> task_source,
                      TaskPriority priority) override;

  void SetWorkerPool(SchedulerWorkerPool* worker_pool);

 private:
  const TrackedRef<TaskTracker> task_tracker_;
  DelayedTaskManager* const delayed_task_manager_;
  SchedulerWorkerPool* worker_pool_ = nullptr;
};

// An enumeration of possible thread pool TaskRunner types. Used to
// parametrize relevant thread_pool tests.
// TODO(etiennep): Migrate to TaskSourceExecutionMode.
enum class ExecutionMode { PARALLEL, SEQUENCED, SINGLE_THREADED };

// An enumeration of possible pool types. Used to parametrize relevant
// thread_pool tests.
enum class PoolType {
  GENERIC,
#if defined(OS_WIN) || defined(OS_MACOSX)
  NATIVE,
#endif
};

// Creates a Sequence with given |traits| and pushes |task| to it. If a
// TaskRunner is associated with |task|, it should be be passed as |task_runner|
// along with its |execution_mode|. Returns the created Sequence.
scoped_refptr<Sequence> CreateSequenceWithTask(
    Task task,
    const TaskTraits& traits,
    scoped_refptr<TaskRunner> task_runner = nullptr,
    TaskSourceExecutionMode execution_mode =
        TaskSourceExecutionMode::kParallel);

// Creates a TaskRunner that posts tasks to the worker pool owned by
// |scheduler_task_runner_delegate| with the |execution_mode| execution mode
// and the WithBaseSyncPrimitives() trait.
// Caveat: this does not support ExecutionMode::SINGLE_THREADED.
scoped_refptr<TaskRunner> CreateTaskRunnerWithExecutionMode(
    test::ExecutionMode execution_mode,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate,
    const TaskTraits& traits = TaskTraits());

scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
    const TaskTraits& traits,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate);

scoped_refptr<SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
    const TaskTraits& traits,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate);

void WaitWithoutBlockingObserver(WaitableEvent* event);

// Calls StartShutdown() and CompleteShutdown() on |task_tracker|.
void ShutdownTaskTracker(TaskTracker* task_tracker);

}  // namespace test
}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_TEST_UTILS_H_
