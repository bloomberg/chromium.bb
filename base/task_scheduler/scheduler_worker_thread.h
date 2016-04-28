// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_H_

#include <memory>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/scheduler_lock.h"
#include "base/task_scheduler/sequence.h"
#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

class TaskTracker;

// A thread that runs Tasks from Sequences returned by a delegate.
//
// A SchedulerWorkerThread starts out sleeping. It is woken up by a call to
// WakeUp(). After a wake-up, a SchedulerWorkerThread runs Tasks from Sequences
// returned by the GetWork() method of its delegate as long as it doesn't return
// nullptr. It also periodically checks with its TaskTracker whether shutdown
// has completed and exits when it has.
//
// This class is thread-safe.
class BASE_EXPORT SchedulerWorkerThread : public PlatformThread::Delegate {
 public:
  // Delegate interface for SchedulerWorkerThread. The methods are always called
  // from the thread managed by the SchedulerWorkerThread instance.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called by |worker_thread| when it enters its main function.
    virtual void OnMainEntry(SchedulerWorkerThread* worker_thread) = 0;

    // Called by |worker_thread| to get a Sequence from which to run a Task.
    virtual scoped_refptr<Sequence> GetWork(
        SchedulerWorkerThread* worker_thread) = 0;

    // Called when |sequence| isn't empty after the SchedulerWorkerThread pops a
    // Task from it. |sequence| is the last Sequence returned by GetWork().
    virtual void ReEnqueueSequence(scoped_refptr<Sequence> sequence) = 0;
  };

  // Creates a SchedulerWorkerThread with priority |thread_priority| that runs
  // Tasks from Sequences returned by |delegate|. |task_tracker| is used to
  // handle shutdown behavior of Tasks. Returns nullptr if creating the
  // underlying platform thread fails.
  static std::unique_ptr<SchedulerWorkerThread> Create(
      ThreadPriority thread_priority,
      std::unique_ptr<Delegate> delegate,
      TaskTracker* task_tracker);

  // Destroying a SchedulerWorkerThread in production is not allowed; it is
  // always leaked. In tests, it can only be destroyed after JoinForTesting()
  // has returned.
  ~SchedulerWorkerThread() override;

  // Wakes up this SchedulerWorkerThread if it wasn't already awake. After this
  // is called, this SchedulerWorkerThread will run Tasks from Sequences
  // returned by the GetWork() method of its delegate until it returns nullptr.
  void WakeUp();

  SchedulerWorkerThread::Delegate* delegate() { return delegate_.get(); }

  // Joins this SchedulerWorkerThread. If a Task is already running, it will be
  // allowed to complete its execution. This can only be called once.
  void JoinForTesting();

 private:
  SchedulerWorkerThread(ThreadPriority thread_priority,
                        std::unique_ptr<Delegate> delegate,
                        TaskTracker* task_tracker);

  // PlatformThread::Delegate:
  void ThreadMain() override;

  bool ShouldExitForTesting() const;

  // Platform thread managed by this SchedulerWorkerThread.
  PlatformThreadHandle thread_handle_;

  // Event signaled to wake up this SchedulerWorkerThread.
  WaitableEvent wake_up_event_;

  const std::unique_ptr<Delegate> delegate_;
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
