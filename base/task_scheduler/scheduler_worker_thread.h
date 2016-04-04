// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_H_

#include <memory>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/sequence.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

class TaskTracker;

// A thread that runs Tasks from Sequences returned by a callback.
//
// A SchedulerWorkerThread is woken up when its WakeUp() method is called. After
// a wake- up, a SchedulerWorkerThread runs Tasks from Sequences returned by its
// "get work" callback as long as it doesn't return nullptr. It also
// periodically checks with its TaskTracker whether shutdown has completed and
// exits when it has.
//
// This class is thread-safe.
class BASE_EXPORT SchedulerWorkerThread : public PlatformThread::Delegate {
 public:
  // Callback invoked to get a Sequence from which to run a Task on
  // |worker_thread|.
  using GetWorkCallback =
      Callback<scoped_refptr<Sequence>(SchedulerWorkerThread* worker_thread)>;

  // Callback invoked after |worker_thread| has tried to run a Task from
  // |sequence| (a TaskTracker might have prevented the Task from running).
  using RanTaskFromSequenceCallback =
      Callback<void(const SchedulerWorkerThread* worker_thread,
                    scoped_refptr<Sequence> sequence)>;

  // Creates a SchedulerWorkerThread with priority |thread_priority| that runs
  // Tasks from Sequences returned by |get_work_callback|. |main_entry_callback|
  // is invoked when the main function of the SchedulerWorkerThread is entered.
  // |ran_task_from_sequence_callback| is invoked after the
  // SchedulerWorkerThread has tried to run a Task from a Sequence returned by
  // |get_work_callback|. |task_tracker| is used to handle shutdown behavior of
  // Tasks. Returns nullptr if creating the underlying platform thread fails.
  static std::unique_ptr<SchedulerWorkerThread> CreateSchedulerWorkerThread(
      ThreadPriority thread_priority,
      const Closure& main_entry_callback,
      const GetWorkCallback& get_work_callback,
      const RanTaskFromSequenceCallback& ran_task_from_sequence_callback,
      TaskTracker* task_tracker);

  // Destroying a SchedulerWorkerThread in production is not allowed; it is
  // always leaked. In tests, it can only be destroyed after JoinForTesting()
  // has returned.
  ~SchedulerWorkerThread() override;

  // Wakes up this SchedulerWorkerThread. After this is called, this
  // SchedulerWorkerThread will run Tasks from Sequences returned by
  // |get_work_callback_| until it returns nullptr.
  void WakeUp();

  // Joins this SchedulerWorkerThread. If a Task is already running, it will be
  // allowed to complete its execution. This can only be called once.
  void JoinForTesting();

 private:
  SchedulerWorkerThread(
      ThreadPriority thread_priority,
      const Closure& main_entry_callback,
      const GetWorkCallback& get_work_callback,
      const RanTaskFromSequenceCallback& ran_task_from_sequence_callback,
      TaskTracker* task_tracker);

  // PlatformThread::Delegate:
  void ThreadMain() override;

  bool ShouldExitForTesting() const;

  // Platform thread managed by this SchedulerWorkerThread.
  PlatformThreadHandle thread_handle_;

  // Event signaled to wake up this SchedulerWorkerThread.
  WaitableEvent wake_up_event_;

  const Closure main_entry_callback_;
  const GetWorkCallback get_work_callback_;
  const RanTaskFromSequenceCallback ran_task_from_sequence_callback_;
  TaskTracker* const task_tracker_;

  // Synchronizes access to |should_exit_for_testing_|.
  mutable SchedulerLock should_exit_for_testing_lock_;

  // True once JoinForTesting() has been called.
  bool should_exit_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerThread);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_H_
