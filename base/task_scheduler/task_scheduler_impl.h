// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_TASK_SCHEDULER_IMPL_H_
#define BASE_TASK_SCHEDULER_TASK_SCHEDULER_IMPL_H_

#include <memory>

#include "base/base_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/task_scheduler/delayed_task_manager.h"
#include "base/task_scheduler/scheduler_thread_pool_impl.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_tracker.h"
#include "base/task_scheduler/task_traits.h"

namespace base {
namespace internal {

// Default TaskScheduler implementation. This class is thread-safe.
class BASE_EXPORT TaskSchedulerImpl : public TaskScheduler {
 public:
  // Creates and returns an initialized TaskSchedulerImpl. CHECKs on failure to
  // do so (never returns null).
  static std::unique_ptr<TaskSchedulerImpl> Create();

  // Destroying a TaskSchedulerImpl is not allowed in production; it is always
  // leaked. In tests, it can only be destroyed after JoinForTesting() has
  // returned.
  ~TaskSchedulerImpl() override;

  // TaskScheduler:
  void PostTaskWithTraits(const tracked_objects::Location& from_here,
                          const TaskTraits& traits,
                          const Closure& task) override;
  scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
      const TaskTraits& traits,
      ExecutionMode execution_mode) override;
  void Shutdown() override;

  // Joins all threads of this scheduler. Tasks that are already running are
  // allowed to complete their execution. This can only be called once.
  void JoinForTesting();

 private:
  TaskSchedulerImpl();

  void Initialize();

  // Returns the thread pool that runs Tasks with |traits|.
  SchedulerThreadPool* GetThreadPoolForTraits(const TaskTraits& traits);

  // Callback invoked when a non-single-thread |sequence| isn't empty after a
  // worker thread pops a Task from it.
  void ReEnqueueSequenceCallback(scoped_refptr<Sequence> sequence);

  TaskTracker task_tracker_;
  DelayedTaskManager delayed_task_manager_;

  // Thread pool for BACKGROUND Tasks without file I/O.
  std::unique_ptr<SchedulerThreadPoolImpl> background_thread_pool_;

  // Thread pool for BACKGROUND Tasks with file I/O.
  std::unique_ptr<SchedulerThreadPoolImpl> background_file_io_thread_pool_;

  // Thread pool for USER_VISIBLE and USER_BLOCKING Tasks without file I/O.
  std::unique_ptr<SchedulerThreadPoolImpl> normal_thread_pool_;

  // Thread pool for USER_VISIBLE and USER_BLOCKING Tasks with file I/O.
  std::unique_ptr<SchedulerThreadPoolImpl> normal_file_io_thread_pool_;

#if DCHECK_IS_ON()
  // Signaled once JoinForTesting() has returned.
  WaitableEvent join_for_testing_returned_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerImpl);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_TASK_SCHEDULER_IMPL_H_
