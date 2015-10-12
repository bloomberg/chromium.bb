// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
#define CONTENT_RENDERER_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_

#include <set>

#include "base/pending_task.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/base/lazy_now.h"
#include "components/scheduler/base/task_queue.h"
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

  class SCHEDULER_EXPORT Task : public base::PendingTask {
   public:
    Task();
    Task(const tracked_objects::Location& posted_from,
         const base::Closure& task,
         int sequence_number,
         bool nestable);

    int enqueue_order() const {
#ifndef NDEBUG
      DCHECK(enqueue_order_set_);
#endif
      return enqueue_order_;
    }

    void set_enqueue_order(int enqueue_order) {
#ifndef NDEBUG
      DCHECK(!enqueue_order_set_);
      enqueue_order_set_ = true;
#endif
      enqueue_order_ = enqueue_order;
    }

   private:
#ifndef NDEBUG
    bool enqueue_order_set_;
#endif
    // Similar to sequence number, but the |enqueue_order| is set by
    // EnqueueTasksLocked and is not initially defined for delayed tasks until
    // they are enqueued on the |incoming_queue_|.
    int enqueue_order_;
  };

  // TaskQueue implementation.
  void UnregisterTaskQueue() override;
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
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;

  bool NextPendingDelayedTaskRunTime(
      base::TimeTicks* next_pending_delayed_task);

  void UpdateWorkQueue(LazyNow* lazy_now,
                       bool should_trigger_wakeup,
                       const Task* previous_task);
  Task TakeTaskFromWorkQueue();

  std::queue<Task>& work_queue() { return main_thread_only().work_queue; }

  WakeupPolicy wakeup_policy() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return wakeup_policy_;
  }

  const char* GetName() const override;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  size_t get_task_queue_set_index() const {
    return main_thread_only().set_index;
  }

  void set_task_queue_set_index(size_t set_index) {
    main_thread_only().set_index = set_index;
  }

  // If the work queue isn't empty, |enqueue_order| gets set to the enqueue
  // order of the front task and the function returns true.  Otherwise the
  // function returns false.
  bool GetWorkQueueFrontTaskEnqueueOrder(int* enqueue_order) const;

  bool GetQuiescenceMonitored() const { return should_monitor_quiescence_; }
  bool GetShouldNotifyObservers() const {
    return should_notify_observers_;
  }

  void NotifyWillProcessTask(const base::PendingTask& pending_task);
  void NotifyDidProcessTask(const base::PendingTask& pending_task);

  // Delayed task posted to the underlying run loop, which locks
  // |any_thread_lock_| and calls MoveReadyDelayedTasksToIncomingQueueLocked to
  // process dealyed tasks
  // that need to be run now.  Thread safe, but in practice it's always called
  // from the main thread.
  void MoveReadyDelayedTasksToIncomingQueue(LazyNow* lazy_now);

  // Test support functions.  These should not be used in production code.
  void PushTaskOntoWorkQueueForTest(const Task& task);
  void PopTaskFromWorkQueueForTest();
  size_t WorkQueueSizeForTest() const {
    return main_thread_only().work_queue.size();
  }

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

  struct AnyThread {
    AnyThread(TaskQueueManager* task_queue_manager, PumpPolicy pump_policy);
    ~AnyThread();

    // TaskQueueManager is maintained in two copies: inside AnyThread and inside
    // MainThreadOnly. It can be changed only from main thread, so it should be
    // locked before accessing from other threads.
    TaskQueueManager* task_queue_manager;

    std::queue<Task> incoming_queue;
    PumpPolicy pump_policy;
    std::priority_queue<Task> delayed_task_queue;
  };

  struct MainThreadOnly {
    explicit MainThreadOnly(TaskQueueManager* task_queue_manager);
    ~MainThreadOnly();

    // Another copy of TaskQueueManager for lock-free access from the main
    // thread. See description inside struct AnyThread for details.
    TaskQueueManager* task_queue_manager;

    std::queue<Task> work_queue;
    base::ObserverList<base::MessageLoop::TaskObserver> task_observers;
    size_t set_index;
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

  // Enqueues any delayed tasks which should be run now on the incoming_queue_
  // and calls ScheduleDelayedWorkLocked to ensure future tasks are scheduled.
  // Must be called with |any_thread_lock_| locked.
  void MoveReadyDelayedTasksToIncomingQueueLocked(LazyNow* lazy_now);

  void PumpQueueLocked();
  bool TaskIsOlderThanQueuedTasks(const Task* task);
  bool ShouldAutoPumpQueueLocked(bool should_trigger_wakeup,
                                 const Task* previous_task);

  // Push the task onto the |incoming_queue_| and for auto pumped queues it
  // calls MaybePostDoWorkOnMainRunner if the incomming queue was empty.
  void EnqueueTaskLocked(const Task& pending_task);

  // Push the task onto the |incoming_queue_| and allocates an
  // enqueue_order for it based on |enqueue_order_policy|.  Does not call
  // MaybePostDoWorkOnMainRunner!
  void EnqueueDelayedTaskLocked(const Task& pending_task);

  void TraceQueueSize(bool is_locked) const;
  static void QueueAsValueInto(const std::queue<Task>& queue,
                               base::trace_event::TracedValue* state);
  static void QueueAsValueInto(const std::priority_queue<Task>& queue,
                               base::trace_event::TracedValue* state);
  static void TaskAsValueInto(const Task& task,
                              base::trace_event::TracedValue* state);

  const base::PlatformThreadId thread_id_;

  mutable base::Lock any_thread_lock_;
  AnyThread any_thread_;
  struct AnyThread& any_thread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& any_thread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  const char* name_;
  const char* disabled_by_default_tracing_category_;
  const char* disabled_by_default_verbose_tracing_category_;

  base::ThreadChecker main_thread_checker_;
  MainThreadOnly main_thread_only_;
  MainThreadOnly& main_thread_only() {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return main_thread_only_;
  }
  const MainThreadOnly& main_thread_only() const {
    DCHECK(main_thread_checker_.CalledOnValidThread());
    return main_thread_only_;
  }

  const WakeupPolicy wakeup_policy_;
  const bool should_monitor_quiescence_;
  const bool should_notify_observers_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueImpl);
};

}  // namespace internal
}  // namespace scheduler

#endif  // CONTENT_RENDERER_SCHEDULER_BASE_TASK_QUEUE_IMPL_H_
