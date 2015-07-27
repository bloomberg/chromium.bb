// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_IMPL_H_
#define CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_IMPL_H_

#include <set>

#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/child/lazy_now.h"
#include "components/scheduler/child/task_queue.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {
class TaskQueueManager;

namespace internal {

class SCHEDULER_EXPORT TaskQueueImpl final : public TaskQueue {
 public:
  TaskQueueImpl(TaskQueueManager* task_queue_manager,
                const Spec& spec,
                const char* disabled_by_default_tracing_category,
                const char* disabled_by_default_verbose_tracing_category);

  // TaskQueue implementation.
  bool RunsTasksOnCurrentThread() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool PostDelayedTaskAt(const tracked_objects::Location& from_here,
                         const base::Closure& task,
                         base::TimeTicks desired_run_time) override;

  bool IsQueueEnabled() const override;
  QueueState GetQueueState() const override;
  void SetQueuePriority(QueuePriority priority) override;
  void PumpQueue() override;
  void SetPumpPolicy(PumpPolicy pump_policy) override;

  bool NextPendingDelayedTaskRunTime(
      base::TimeTicks* next_pending_delayed_task);

  void UpdateWorkQueue(LazyNow* lazy_now,
                       bool should_trigger_wakeup,
                       const base::PendingTask* previous_task);
  base::PendingTask TakeTaskFromWorkQueue();

  void WillDeleteTaskQueueManager();

  base::TaskQueue& work_queue() { return work_queue_; }

  WakeupPolicy wakeup_policy() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return wakeup_policy_;
  }

  const char* GetName() const override;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  size_t get_task_queue_set_index() const { return set_index_; }

  void set_task_queue_set_index(size_t set_index) { set_index_ = set_index; }

  // If the work queue isn't empty, |age| gets set to the sequence number of the
  // front task and the dunctio returns true.  Otherwise the function returns
  // false.
  bool GetWorkQueueFrontTaskAge(int* age) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const { return should_notify_observers_; }

  // Test support functions.  These should not be used in production code.
  void PushTaskOntoWorkQueueForTest(const base::PendingTask& task);
  void PopTaskFromWorkQueueForTest();
  size_t WorkQueueSizeForTest() const { return work_queue_.size(); }

  // Can be called on any thread.
  static const char* PumpPolicyToString(TaskQueue::PumpPolicy pump_policy);

  // Can be called on any thread.
  static const char* WakeupPolicyToString(
      TaskQueue::WakeupPolicy wakeup_policy);

  // Can be called on any thread.
  static const char* PriorityToString(TaskQueue::QueuePriority priority);

 private:
  enum class TaskType {
    NORMAL,
    NON_NESTABLE,
  };

  ~TaskQueueImpl() override;

  bool PostDelayedTaskImpl(const tracked_objects::Location& from_here,
                           const base::Closure& task,
                           base::TimeDelta delay,
                           TaskType task_type);
  bool PostDelayedTaskLocked(LazyNow* lazy_now,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task,
                             base::TimeTicks desired_run_time,
                             TaskType task_type);

  // Delayed task posted to the underlying run loop, which locks |lock_| and
  // calls MoveReadyDelayedTasksToIncomingQueueLocked to process dealyed tasks
  // that need to be run now.
  void MoveReadyDelayedTasksToIncomingQueue();

  // Enqueues any delayed tasks which should be run now on the incoming_queue_
  // and calls ScheduleDelayedWorkLocked to ensure future tasks are scheduled.
  // Must be called with |lock_| locked.
  void MoveReadyDelayedTasksToIncomingQueueLocked(LazyNow* lazy_now);

  // Posts MoveReadyDelayedTasksToIncomingQueue if there isn't already a task
  // posted on the underlying runloop for the next task's scheduled run time.
  void ScheduleDelayedWorkLocked(LazyNow* lazy_now);

  void PumpQueueLocked();
  bool TaskIsOlderThanQueuedTasks(const base::PendingTask* task);
  bool ShouldAutoPumpQueueLocked(bool should_trigger_wakeup,
                                 const base::PendingTask* previous_task);

  // Push the task onto the |incoming_queue_| and allocate a sequence number
  // for it.
  void EnqueueTaskLocked(const base::PendingTask& pending_task);

  void TraceQueueSize(bool is_locked) const;
  static void QueueAsValueInto(const base::TaskQueue& queue,
                               base::trace_event::TracedValue* state);
  static void QueueAsValueInto(const base::DelayedTaskQueue& queue,
                               base::trace_event::TracedValue* state);
  static void TaskAsValueInto(const base::PendingTask& task,
                              base::trace_event::TracedValue* state);

  // This lock protects all members in the contigious block below.
  // TODO(alexclarke): Group all the members protected by the lock into a struct
  mutable base::Lock lock_;
  base::PlatformThreadId thread_id_;
  TaskQueueManager* task_queue_manager_;
  base::TaskQueue incoming_queue_;
  PumpPolicy pump_policy_;
  // Queue-local task sequence number for maintaining the order of delayed
  // tasks which are posted for the exact same time. Note that this will be
  // replaced by the global sequence number when the delay has elapsed.
  int delayed_task_sequence_number_;
  base::DelayedTaskQueue delayed_task_queue_;
  std::set<base::TimeTicks> in_flight_kick_delayed_tasks_;

  const char* name_;
  const char* disabled_by_default_tracing_category_;
  const char* disabled_by_default_verbose_tracing_category_;

  base::ThreadChecker main_thread_checker_;
  base::TaskQueue work_queue_;
  WakeupPolicy wakeup_policy_;
  size_t set_index_;
  bool should_monitor_quiescence_;
  bool should_notify_observers_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueImpl);
};

}  // namespace internal
}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_TASK_QUEUE_IMPL_H_
