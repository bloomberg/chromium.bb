// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_impl.h"

#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"

namespace scheduler {
namespace internal {

TaskQueueImpl::TaskQueueImpl(
    TaskQueueManager* task_queue_manager,
    const Spec& spec,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : thread_id_(base::PlatformThread::CurrentId()),
      any_thread_(task_queue_manager, spec.pump_policy),
      name_(spec.name),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      main_thread_only_(task_queue_manager),
      wakeup_policy_(spec.wakeup_policy),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers) {}

TaskQueueImpl::~TaskQueueImpl() {}

TaskQueueImpl::Task::Task()
    : PendingTask(tracked_objects::Location(),
                  base::Closure(),
                  base::TimeTicks(),
                  true),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = 0;
}

TaskQueueImpl::Task::Task(const tracked_objects::Location& posted_from,
                          const base::Closure& task,
                          int sequence_number,
                          bool nestable)
    : PendingTask(posted_from, task, base::TimeTicks(), nestable),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  sequence_num = sequence_number;
}

TaskQueueImpl::AnyThread::AnyThread(TaskQueueManager* task_queue_manager,
                                    PumpPolicy pump_policy)
    : task_queue_manager(task_queue_manager), pump_policy(pump_policy) {}

TaskQueueImpl::AnyThread::~AnyThread() {}

TaskQueueImpl::MainThreadOnly::MainThreadOnly(
    TaskQueueManager* task_queue_manager)
    : task_queue_manager(task_queue_manager),
      set_index(0) {}

TaskQueueImpl::MainThreadOnly::~MainThreadOnly() {}

void TaskQueueImpl::UnregisterTaskQueue() {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return;
  any_thread().task_queue_manager->UnregisterTaskQueue(this);

  any_thread().task_queue_manager = nullptr;
  main_thread_only().task_queue_manager = nullptr;
  any_thread().delayed_task_queue = std::priority_queue<Task>();
  any_thread().incoming_queue = std::queue<Task>();
  main_thread_only().work_queue = std::queue<Task>();
}

bool TaskQueueImpl::RunsTasksOnCurrentThread() const {
  base::AutoLock lock(any_thread_lock_);
  return base::PlatformThread::CurrentId() == thread_id_;
}

bool TaskQueueImpl::PostDelayedTask(const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NORMAL);
}

bool TaskQueueImpl::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return PostDelayedTaskImpl(from_here, task, delay, TaskType::NON_NESTABLE);
}

bool TaskQueueImpl::PostDelayedTaskAt(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeTicks desired_run_time) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return false;
  LazyNow lazy_now(any_thread().task_queue_manager->delegate().get());
  return PostDelayedTaskLocked(&lazy_now, from_here, task, desired_run_time,
                               TaskType::NORMAL);
}

bool TaskQueueImpl::PostDelayedTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    TaskType task_type) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return false;
  LazyNow lazy_now(any_thread().task_queue_manager->delegate().get());
  base::TimeTicks desired_run_time;
  if (delay > base::TimeDelta())
    desired_run_time = lazy_now.Now() + delay;
  return PostDelayedTaskLocked(&lazy_now, from_here, task, desired_run_time,
                               task_type);
}

bool TaskQueueImpl::PostDelayedTaskLocked(
    LazyNow* lazy_now,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeTicks desired_run_time,
    TaskType task_type) {
  DCHECK(any_thread().task_queue_manager);
  Task pending_task(from_here, task,
                    any_thread().task_queue_manager->GetNextSequenceNumber(),
                    task_type != TaskType::NON_NESTABLE);
  any_thread().task_queue_manager->DidQueueTask(pending_task);

  if (!desired_run_time.is_null()) {
    pending_task.delayed_run_time = std::max(lazy_now->Now(), desired_run_time);
    // TODO(alexclarke): consider emplace() when C++11 library features allowed.
    any_thread().delayed_task_queue.push(pending_task);
    TraceQueueSize(true);
    // Schedule a later call to MoveReadyDelayedTasksToIncomingQueue.
    any_thread().task_queue_manager->ScheduleDelayedWork(this, desired_run_time,
                                                         lazy_now);
    return true;
  }
  pending_task.set_enqueue_order(pending_task.sequence_num);
  EnqueueTaskLocked(pending_task);
  return true;
}

void TaskQueueImpl::MoveReadyDelayedTasksToIncomingQueue(LazyNow* lazy_now) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return;

  MoveReadyDelayedTasksToIncomingQueueLocked(lazy_now);
}

void TaskQueueImpl::MoveReadyDelayedTasksToIncomingQueueLocked(
    LazyNow* lazy_now) {
  // Enqueue all delayed tasks that should be running now.
  while (!any_thread().delayed_task_queue.empty() &&
         any_thread().delayed_task_queue.top().delayed_run_time <=
             lazy_now->Now()) {
    // TODO(alexclarke): consider std::move() when allowed.
    EnqueueDelayedTaskLocked(any_thread().delayed_task_queue.top());
    any_thread().delayed_task_queue.pop();
  }
  TraceQueueSize(true);
}

bool TaskQueueImpl::IsQueueEnabled() const {
  if (!main_thread_only().task_queue_manager)
    return false;

  return main_thread_only().task_queue_manager->selector_.IsQueueEnabled(this);
}

TaskQueue::QueueState TaskQueueImpl::GetQueueState() const {
  if (!main_thread_only().work_queue.empty())
    return QueueState::HAS_WORK;

  {
    base::AutoLock lock(any_thread_lock_);
    if (any_thread().incoming_queue.empty()) {
      return QueueState::EMPTY;
    } else {
      return QueueState::NEEDS_PUMPING;
    }
  }
}

bool TaskQueueImpl::TaskIsOlderThanQueuedTasks(const Task* task) {
  // A null task is passed when UpdateQueue is called before any task is run.
  // In this case we don't want to pump an after_wakeup queue, so return true
  // here.
  if (!task)
    return true;

  // Return false if there are no task in the incoming queue.
  if (any_thread().incoming_queue.empty())
    return false;

  const TaskQueueImpl::Task& oldest_queued_task =
      any_thread().incoming_queue.front();
  return task->enqueue_order() < oldest_queued_task.enqueue_order();
}

bool TaskQueueImpl::ShouldAutoPumpQueueLocked(bool should_trigger_wakeup,
                                              const Task* previous_task) {
  if (any_thread().pump_policy == PumpPolicy::MANUAL)
    return false;
  if (any_thread().pump_policy == PumpPolicy::AFTER_WAKEUP &&
      (!should_trigger_wakeup || TaskIsOlderThanQueuedTasks(previous_task)))
    return false;
  if (any_thread().incoming_queue.empty())
    return false;
  return true;
}

bool TaskQueueImpl::NextPendingDelayedTaskRunTime(
    base::TimeTicks* next_pending_delayed_task) {
  base::AutoLock lock(any_thread_lock_);
  if (any_thread().delayed_task_queue.empty())
    return false;
  *next_pending_delayed_task =
      any_thread().delayed_task_queue.top().delayed_run_time;
  return true;
}

void TaskQueueImpl::UpdateWorkQueue(LazyNow* lazy_now,
                                    bool should_trigger_wakeup,
                                    const Task* previous_task) {
  DCHECK(main_thread_only().work_queue.empty());
  base::AutoLock lock(any_thread_lock_);
  if (!ShouldAutoPumpQueueLocked(should_trigger_wakeup, previous_task))
    return;
  MoveReadyDelayedTasksToIncomingQueueLocked(lazy_now);
  std::swap(main_thread_only().work_queue, any_thread().incoming_queue);
  // |any_thread().incoming_queue| is now empty so
  // TaskQueueManager::UpdateQueues no longer needs to consider
  // this queue for reloading.
  any_thread().task_queue_manager->UnregisterAsUpdatableTaskQueue(this);
  if (!main_thread_only().work_queue.empty()) {
    DCHECK(any_thread().task_queue_manager);
    any_thread().task_queue_manager->selector_.GetTaskQueueSets()->OnPushQueue(
        this);
    TraceQueueSize(true);
  }
}

TaskQueueImpl::Task TaskQueueImpl::TakeTaskFromWorkQueue() {
  // TODO(alexclarke): consider std::move() when allowed.
  Task pending_task = main_thread_only().work_queue.front();
  main_thread_only().work_queue.pop();
  DCHECK(main_thread_only().task_queue_manager);
  main_thread_only()
      .task_queue_manager->selector_.GetTaskQueueSets()
      ->OnPopQueue(this);
  TraceQueueSize(false);
  return pending_task;
}

void TaskQueueImpl::TraceQueueSize(bool is_locked) const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(disabled_by_default_tracing_category_,
                                     &is_tracing);
  if (!is_tracing)
    return;
  if (!is_locked)
    any_thread_lock_.Acquire();
  else
    any_thread_lock_.AssertAcquired();
  TRACE_COUNTER1(disabled_by_default_tracing_category_, GetName(),
                 any_thread().incoming_queue.size() +
                     main_thread_only().work_queue.size() +
                     any_thread().delayed_task_queue.size());
  if (!is_locked)
    any_thread_lock_.Release();
}

void TaskQueueImpl::EnqueueTaskLocked(const Task& pending_task) {
  if (!any_thread().task_queue_manager)
    return;
  if (any_thread().incoming_queue.empty())
    any_thread().task_queue_manager->RegisterAsUpdatableTaskQueue(this);
  if (any_thread().pump_policy == PumpPolicy::AUTO &&
      any_thread().incoming_queue.empty()) {
    any_thread().task_queue_manager->MaybePostDoWorkOnMainRunner();
  }
  // TODO(alexclarke): consider std::move() when allowed.
  any_thread().incoming_queue.push(pending_task);
  TraceQueueSize(true);
}

void TaskQueueImpl::EnqueueDelayedTaskLocked(const Task& pending_task) {
  if (!any_thread().task_queue_manager)
    return;
  if (any_thread().incoming_queue.empty())
    any_thread().task_queue_manager->RegisterAsUpdatableTaskQueue(this);
  // TODO(alexclarke): consider std::move() when allowed.
  any_thread().incoming_queue.push(pending_task);
  any_thread().incoming_queue.back().set_enqueue_order(
      any_thread().task_queue_manager->GetNextSequenceNumber());
  TraceQueueSize(true);
}

void TaskQueueImpl::SetPumpPolicy(PumpPolicy pump_policy) {
  base::AutoLock lock(any_thread_lock_);
  if (pump_policy == PumpPolicy::AUTO &&
      any_thread().pump_policy != PumpPolicy::AUTO) {
    PumpQueueLocked();
  }
  any_thread().pump_policy = pump_policy;
}

void TaskQueueImpl::PumpQueueLocked() {
  if (!any_thread().task_queue_manager)
    return;

  LazyNow lazy_now(any_thread().task_queue_manager->delegate().get());
  MoveReadyDelayedTasksToIncomingQueueLocked(&lazy_now);

  bool was_empty = main_thread_only().work_queue.empty();
  while (!any_thread().incoming_queue.empty()) {
    // TODO(alexclarke): consider std::move() when allowed.
    main_thread_only().work_queue.push(any_thread().incoming_queue.front());
    any_thread().incoming_queue.pop();
  }
  // |incoming_queue| is now empty so TaskQueueManager::UpdateQueues no longer
  // needs to consider this queue for reloading.
  any_thread().task_queue_manager->UnregisterAsUpdatableTaskQueue(this);
  if (!main_thread_only().work_queue.empty()) {
    if (was_empty) {
      any_thread()
          .task_queue_manager->selector_.GetTaskQueueSets()
          ->OnPushQueue(this);
    }
    any_thread().task_queue_manager->MaybePostDoWorkOnMainRunner();
  }
}

void TaskQueueImpl::PumpQueue() {
  base::AutoLock lock(any_thread_lock_);
  PumpQueueLocked();
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

bool TaskQueueImpl::GetWorkQueueFrontTaskEnqueueOrder(
    int* enqueue_order) const {
  if (main_thread_only().work_queue.empty())
    return false;
  *enqueue_order = main_thread_only().work_queue.front().enqueue_order();
  return true;
}

void TaskQueueImpl::PushTaskOntoWorkQueueForTest(const Task& task) {
  main_thread_only().work_queue.push(task);
}

void TaskQueueImpl::PopTaskFromWorkQueueForTest() {
  main_thread_only().work_queue.pop();
}

void TaskQueueImpl::SetQueuePriority(QueuePriority priority) {
  if (!main_thread_only().task_queue_manager)
    return;

  main_thread_only().task_queue_manager->selector_.SetQueuePriority(this,
                                                                    priority);
}

// static
const char* TaskQueueImpl::PumpPolicyToString(
    TaskQueue::PumpPolicy pump_policy) {
  switch (pump_policy) {
    case TaskQueue::PumpPolicy::AUTO:
      return "auto";
    case TaskQueue::PumpPolicy::AFTER_WAKEUP:
      return "after_wakeup";
    case TaskQueue::PumpPolicy::MANUAL:
      return "manual";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* TaskQueueImpl::WakeupPolicyToString(
    TaskQueue::WakeupPolicy wakeup_policy) {
  switch (wakeup_policy) {
    case TaskQueue::WakeupPolicy::CAN_WAKE_OTHER_QUEUES:
      return "can_wake_other_queues";
    case TaskQueue::WakeupPolicy::DONT_WAKE_OTHER_QUEUES:
      return "dont_wake_other_queues";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* TaskQueueImpl::PriorityToString(QueuePriority priority) {
  switch (priority) {
    case CONTROL_PRIORITY:
      return "control";
    case HIGH_PRIORITY:
      return "high";
    case NORMAL_PRIORITY:
      return "normal";
    case BEST_EFFORT_PRIORITY:
      return "best_effort";
    case DISABLED_PRIORITY:
      return "disabled";
    default:
      NOTREACHED();
      return nullptr;
  }
}

void TaskQueueImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  base::AutoLock lock(any_thread_lock_);
  state->BeginDictionary();
  state->SetString("name", GetName());
  state->SetString("pump_policy", PumpPolicyToString(any_thread().pump_policy));
  state->SetString("wakeup_policy", WakeupPolicyToString(wakeup_policy_));
  bool verbose_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      disabled_by_default_verbose_tracing_category_, &verbose_tracing_enabled);
  state->SetInteger("incoming_queue_size", any_thread().incoming_queue.size());
  state->SetInteger("work_queue_size", main_thread_only().work_queue.size());
  state->SetInteger("delayed_task_queue_size",
                    any_thread().delayed_task_queue.size());
  if (verbose_tracing_enabled) {
    state->BeginArray("incoming_queue");
    QueueAsValueInto(any_thread().incoming_queue, state);
    state->EndArray();
    state->BeginArray("work_queue");
    QueueAsValueInto(main_thread_only().work_queue, state);
    state->EndArray();
    state->BeginArray("delayed_task_queue");
    QueueAsValueInto(any_thread().delayed_task_queue, state);
    state->EndArray();
  }
  state->SetString("priority", PriorityToString(static_cast<QueuePriority>(
                                   main_thread_only().set_index)));
  state->EndDictionary();
}

void TaskQueueImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.AddObserver(task_observer);
}

void TaskQueueImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.RemoveObserver(task_observer);
}

void TaskQueueImpl::NotifyWillProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver,
                    main_thread_only().task_observers,
                    WillProcessTask(pending_task));
}

void TaskQueueImpl::NotifyDidProcessTask(
    const base::PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver,
                    main_thread_only().task_observers,
                    DidProcessTask(pending_task));
}

// static
void TaskQueueImpl::QueueAsValueInto(const std::queue<Task>& queue,
                                     base::trace_event::TracedValue* state) {
  std::queue<Task> queue_copy(queue);
  while (!queue_copy.empty()) {
    TaskAsValueInto(queue_copy.front(), state);
    queue_copy.pop();
  }
}

// static
void TaskQueueImpl::QueueAsValueInto(const std::priority_queue<Task>& queue,
                                     base::trace_event::TracedValue* state) {
  std::priority_queue<Task> queue_copy(queue);
  while (!queue_copy.empty()) {
    TaskAsValueInto(queue_copy.top(), state);
    queue_copy.pop();
  }
}

// static
void TaskQueueImpl::TaskAsValueInto(const Task& task,
                                    base::trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("posted_from", task.posted_from.ToString());
  state->SetInteger("enqueue_order", task.enqueue_order());
  state->SetInteger("sequence_num", task.sequence_num);
  state->SetBoolean("nestable", task.nestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetDouble(
      "delayed_run_time",
      (task.delayed_run_time - base::TimeTicks()).InMicroseconds() / 1000.0L);
  state->EndDictionary();
}

}  // namespace internal
}  // namespace scheduler
