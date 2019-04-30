// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_TASK_TRACKER_H_
#define BASE_TASK_THREAD_POOL_TASK_TRACKER_H_

#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <queue>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/checked_lock.h"
#include "base/task/common/task_annotator.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/tracked_ref.h"

namespace base {

class ConditionVariable;
class HistogramBase;

namespace internal {

// Determines which tasks are allowed to run.
enum class CanRunPolicy {
  // All tasks are allowed to run.
  kAll,
  // Only USER_VISIBLE and USER_BLOCKING tasks are allowed to run.
  kForegroundOnly,
  // No tasks can run.
  kNone,
};

// TaskTracker enforces policies that determines whether:
// - A task can be added to a task source (WillPostTask).
// - Tasks for a given priority can run (CanRunPriority).
// - The next task in a scheduled task source can run (RunAndPopNextTask).
// TaskTracker also sets up the environment to run a task (RunAndPopNextTask)
// and records metrics and trace events. This class is thread-safe.
class BASE_EXPORT TaskTracker {
 public:
  // |histogram_label| is used as a suffix for histograms, it must not be empty.
  TaskTracker(StringPiece histogram_label);

  virtual ~TaskTracker();

  // Initiates shutdown. Once this is called, only BLOCK_SHUTDOWN tasks will
  // start running (doesn't affect tasks that are already running). This can
  // only be called once.
  void StartShutdown();

  // Synchronously completes shutdown. StartShutdown() must be called first.
  // Returns when:
  // - All SKIP_ON_SHUTDOWN tasks that were already running have completed their
  //   execution.
  // - All posted BLOCK_SHUTDOWN tasks have completed their execution.
  // CONTINUE_ON_SHUTDOWN tasks still may be running after Shutdown returns.
  // This can only be called once.
  void CompleteShutdown();

  // Waits until there are no incomplete undelayed tasks. May be called in tests
  // to validate that a condition is met after all undelayed tasks have run.
  //
  // Does not wait for delayed tasks. Waits for undelayed tasks posted from
  // other threads during the call. Returns immediately when shutdown completes.
  void FlushForTesting();

  // Returns and calls |flush_callback| when there are no incomplete undelayed
  // tasks. |flush_callback| may be called back on any thread and should not
  // perform a lot of work. May be used when additional work on the current
  // thread needs to be performed during a flush. Only one
  // FlushAsyncForTesting() may be pending at any given time.
  void FlushAsyncForTesting(OnceClosure flush_callback);

  // Sets the new CanRunPolicy policy, possibly affecting result of
  // CanRunPriority(). The caller must wake up worker as appropriate so that
  // tasks that are allowed to run by the new policy can be scheduled.
  void SetCanRunPolicy(CanRunPolicy can_run_policy);

  // Informs this TaskTracker that |task| from a |shutdown_behavior| task source
  // is about to be posted. Returns true if this operation is allowed (|task|
  // should be posted if-and-only-if it is). This method may also modify
  // metadata on |task| if desired.
  bool WillPostTask(Task* task, TaskShutdownBehavior shutdown_behavior);

  // Returns true if a task with |priority| can run under to the current policy.
  bool CanRunPriority(TaskPriority priority) const;

  // Runs the next task in |task_source| unless the current shutdown state
  // prevents that. Then, pops the task from |task_source| (even if it didn't
  // run). Returns |task_source| if non-empty after popping a task from it
  // (which indicates that it should be reenqueued). WillPostTask() must have
  // allowed the task in front of |task_source| to be posted before this is
  // called.
  scoped_refptr<TaskSource> RunAndPopNextTask(
      scoped_refptr<TaskSource> task_source);

  // Returns true once shutdown has started (StartShutdown() was called).
  // Note: sequential consistency with the thread calling StartShutdown() isn't
  // guaranteed by this call.
  bool HasShutdownStarted() const;

  // Returns true if shutdown has completed (StartShutdown() was called and
  // no tasks are blocking shutdown).
  bool IsShutdownComplete() const;

  enum class LatencyHistogramType {
    // Records the latency of each individual task posted through TaskTracker.
    TASK_LATENCY,
    // Records the latency of heartbeat tasks which are independent of current
    // workload. These avoid a bias towards TASK_LATENCY reporting that high-
    // priority tasks are "slower" than regular tasks because high-priority
    // tasks tend to be correlated with heavy workloads.
    HEARTBEAT_LATENCY,
  };

  // Records two histograms
  // 1. ThreadPool.[label].HeartbeatLatencyMicroseconds.[suffix]:
  //    Now() - posted_time
  // 2. ThreadPool.[label].NumTasksRunWhileQueuing.[suffix]:
  //    GetNumTasksRun() - num_tasks_run_when_posted.
  // [label] is the histogram label provided to the constructor.
  // [suffix] is derived from |task_priority| and |may_block|.
  void RecordHeartbeatLatencyAndTasksRunWhileQueuingHistograms(
      TaskPriority task_priority,
      bool may_block,
      TimeTicks posted_time,
      int num_tasks_run_when_posted) const;

  // Returns the number of tasks run so far
  int GetNumTasksRun() const;

  TrackedRef<TaskTracker> GetTrackedRef() {
    return tracked_ref_factory_.GetTrackedRef();
  }

 protected:
  // Runs and deletes |task| if |can_run_task| is true. Otherwise, just deletes
  // |task|. |task| is always deleted in the environment where it runs or would
  // have run. |task_source| is the task source from which |task| was extracted.
  // |traits| are the traits of |task_source|. An override is expected to call
  // its parent's implementation but is free to perform extra work before and
  // after doing so.
  virtual void RunOrSkipTask(Task task,
                             TaskSource* task_source,
                             const TaskTraits& traits,
                             bool can_run_task);

  // Returns true if there are undelayed tasks that haven't completed their
  // execution (still queued or in progress). If it returns false: the side-
  // effects of all completed tasks are guaranteed to be visible to the caller.
  bool HasIncompleteUndelayedTasksForTesting() const;

 private:
  class State;

  void PerformShutdown();

  // Called before WillPostTask() informs the tracing system that a task has
  // been posted. Updates |num_tasks_blocking_shutdown_| if necessary and
  // returns true if the current shutdown state allows the task to be posted.
  bool BeforePostTask(TaskShutdownBehavior effective_shutdown_behavior);

  // Called before a task with |effective_shutdown_behavior| is run by
  // RunTask(). Updates |num_tasks_blocking_shutdown_| if necessary and returns
  // true if the current shutdown state allows the task to be run.
  bool BeforeRunTask(TaskShutdownBehavior effective_shutdown_behavior);

  // Called after a task with |effective_shutdown_behavior| has been run by
  // RunTask(). Updates |num_tasks_blocking_shutdown_| and signals
  // |shutdown_cv_| if necessary.
  void AfterRunTask(TaskShutdownBehavior effective_shutdown_behavior);

  // Called when the number of tasks blocking shutdown becomes zero after
  // shutdown has started.
  void OnBlockingShutdownTasksComplete();

  // Decrements the number of incomplete undelayed tasks and signals |flush_cv_|
  // if it reaches zero.
  void DecrementNumIncompleteUndelayedTasks();

  // Calls |flush_callback_for_testing_| if one is available in a lock-safe
  // manner.
  void CallFlushCallbackForTesting();

  // Records |Now() - posted_time| to the appropriate |latency_histogram_type|
  // based on |task_traits|.
  void RecordLatencyHistogram(LatencyHistogramType latency_histogram_type,
                              TaskTraits task_traits,
                              TimeTicks posted_time) const;

  void IncrementNumTasksRun();

  // Dummy frames to allow identification of shutdown behavior in a stack trace.
  void RunContinueOnShutdown(Task* task);
  void RunSkipOnShutdown(Task* task);
  void RunBlockShutdown(Task* task);
  void RunTaskWithShutdownBehavior(TaskShutdownBehavior shutdown_behavior,
                                   Task* task);

  TaskAnnotator task_annotator_;

  // Number of tasks blocking shutdown and boolean indicating whether shutdown
  // has started.
  const std::unique_ptr<State> state_;

  // Number of undelayed tasks that haven't completed their execution. Is
  // decremented with a memory barrier after a task runs. Is accessed with an
  // acquire memory barrier in FlushForTesting(). The memory barriers ensure
  // that the memory written by flushed tasks is visible when FlushForTesting()
  // returns.
  subtle::Atomic32 num_incomplete_undelayed_tasks_ = 0;

  // Global policy the determines result of CanRunPriority().
  std::atomic<CanRunPolicy> can_run_policy_;

  // Lock associated with |flush_cv_|. Partially synchronizes access to
  // |num_incomplete_undelayed_tasks_|. Full synchronization isn't needed
  // because it's atomic, but synchronization is needed to coordinate waking and
  // sleeping at the right time. Fully synchronizes access to
  // |flush_callback_for_testing_|.
  mutable CheckedLock flush_lock_;

  // Signaled when |num_incomplete_undelayed_tasks_| is or reaches zero or when
  // shutdown completes.
  const std::unique_ptr<ConditionVariable> flush_cv_;

  // Invoked if non-null when |num_incomplete_undelayed_tasks_| is zero or when
  // shutdown completes.
  OnceClosure flush_callback_for_testing_;

  // Synchronizes access to shutdown related members below.
  mutable CheckedLock shutdown_lock_;

  // Event instantiated when shutdown starts and signaled when shutdown
  // completes.
  std::unique_ptr<WaitableEvent> shutdown_event_;

  // Counter for number of tasks run so far, used to record tasks run while
  // a task queued to histogram.
  std::atomic_int num_tasks_run_{0};

  // ThreadPool.TaskLatencyMicroseconds.*,
  // ThreadPool.HeartbeatLatencyMicroseconds.*, and
  // ThreadPool.NumTasksRunWhileQueuing.* histograms. The first index is
  // a TaskPriority. The second index is 0 for non-blocking tasks, 1 for
  // blocking tasks. Intentionally leaked.
  // TODO(scheduler-dev): Consider using STATIC_HISTOGRAM_POINTER_GROUP for
  // these.
  static constexpr int kNumTaskPriorities =
      static_cast<int>(TaskPriority::HIGHEST) + 1;
  HistogramBase* const task_latency_histograms_[kNumTaskPriorities][2];
  HistogramBase* const heartbeat_latency_histograms_[kNumTaskPriorities][2];
  HistogramBase* const
      num_tasks_run_while_queuing_histograms_[kNumTaskPriorities][2];

  // Ensures all state (e.g. dangling cleaned up workers) is coalesced before
  // destroying the TaskTracker (e.g. in test environments).
  // Ref. https://crbug.com/827615.
  TrackedRefFactory<TaskTracker> tracked_ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskTracker);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_TASK_TRACKER_H_
