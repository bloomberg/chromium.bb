// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_impl.h"

#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/task_queue_manager_delegate.h"
#include "components/scheduler/base/time_domain.h"

namespace scheduler {
namespace internal {

TaskQueueImpl::TaskQueueImpl(
    TaskQueueManager* task_queue_manager,
    TimeDomain* time_domain,
    const Spec& spec,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : thread_id_(base::PlatformThread::CurrentId()),
      any_thread_(task_queue_manager, spec.pump_policy, time_domain),
      name_(spec.name),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      main_thread_only_(task_queue_manager),
      wakeup_policy_(spec.wakeup_policy),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers) {
  DCHECK(time_domain);
  time_domain->RegisterQueue(this);
}

TaskQueueImpl::~TaskQueueImpl() {
  base::AutoLock lock(any_thread_lock_);
  if (any_thread().time_domain)
    any_thread().time_domain->UnregisterQueue(this);
}

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
                          base::TimeTicks desired_run_time,
                          int sequence_number,
                          bool nestable)
    : PendingTask(posted_from, task, base::TimeTicks(), nestable),
#ifndef NDEBUG
      enqueue_order_set_(false),
#endif
      enqueue_order_(0) {
  delayed_run_time = desired_run_time;
  sequence_num = sequence_number;
}

TaskQueueImpl::Task::Task(const tracked_objects::Location& posted_from,
                          const base::Closure& task,
                          base::TimeTicks desired_run_time,
                          int sequence_number,
                          bool nestable,
                          int enqueue_order)
    : PendingTask(posted_from, task, base::TimeTicks(), nestable),
#ifndef NDEBUG
      enqueue_order_set_(true),
#endif
      enqueue_order_(enqueue_order) {
  delayed_run_time = desired_run_time;
  sequence_num = sequence_number;
}

TaskQueueImpl::AnyThread::AnyThread(TaskQueueManager* task_queue_manager,
                                    PumpPolicy pump_policy,
                                    TimeDomain* time_domain)
    : task_queue_manager(task_queue_manager),
      pump_policy(pump_policy),
      time_domain(time_domain) {}

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
  if (any_thread().time_domain)
    any_thread().time_domain->UnregisterQueue(this);
  any_thread().time_domain = nullptr;
  any_thread().task_queue_manager->UnregisterTaskQueue(this);

  any_thread().task_queue_manager = nullptr;
  main_thread_only().task_queue_manager = nullptr;
  any_thread().delayed_incoming_queue = std::priority_queue<Task>();
  any_thread().immediate_incoming_queue = std::queue<Task>();
  main_thread_only().immediate_work_queue = std::queue<Task>();
  main_thread_only().delayed_work_queue = std::queue<Task>();
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

bool TaskQueueImpl::PostDelayedTaskImpl(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    TaskType task_type) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return false;
  LazyNow lazy_now(any_thread().time_domain->CreateLazyNow());
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
  int sequence_number =
      any_thread().task_queue_manager->GetNextSequenceNumber();
  if (!desired_run_time.is_null()) {
    PushOntoDelayedIncomingQueueLocked(
        Task(from_here, task, std::max(lazy_now->Now(), desired_run_time),
             sequence_number, task_type != TaskType::NON_NESTABLE),
        lazy_now);
  } else {
    PushOntoImmediateIncomingQueueLocked(
        Task(from_here, task, base::TimeTicks(), sequence_number,
             task_type != TaskType::NON_NESTABLE, sequence_number));
  }
  return true;
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueLocked(
    const Task&& pending_task,
    LazyNow* lazy_now) {
  any_thread().task_queue_manager->DidQueueTask(pending_task);
  any_thread().delayed_incoming_queue.push(pending_task);

  // Schedule a later call to MoveReadyDelayedTasksToDelayedWorkQueue.
  if (base::PlatformThread::CurrentId() == thread_id_) {
    any_thread().time_domain->ScheduleDelayedWork(
        this, pending_task.delayed_run_time, lazy_now);
    TraceQueueSize(true);
  } else {
    // NOTE posting a delayed task from a different thread is not expected to
    // be common. This pathway is less optimal than perhaps it could be
    // because it causes two main thread tasks to be run.  Should this
    // assumption prove to be false in future, we may need to revisit this.
    int thread_hop_task_sequence_number =
        any_thread().task_queue_manager->GetNextSequenceNumber();
    PushOntoImmediateIncomingQueueLocked(Task(
        FROM_HERE,
        base::Bind(&TaskQueueImpl::ScheduleDelayedWorkTask, this,
                   any_thread().time_domain, pending_task.delayed_run_time),
        base::TimeTicks(), thread_hop_task_sequence_number, false,
        thread_hop_task_sequence_number));
  }
}

void TaskQueueImpl::PushOntoImmediateIncomingQueueLocked(
    const Task&& pending_task) {
  if (any_thread().immediate_incoming_queue.empty())
    any_thread().time_domain->RegisterAsUpdatableTaskQueue(this);
  if (any_thread().pump_policy == PumpPolicy::AUTO &&
      any_thread().immediate_incoming_queue.empty()) {
    any_thread().task_queue_manager->MaybePostDoWorkOnMainRunner();
  }
  any_thread().task_queue_manager->DidQueueTask(pending_task);
  any_thread().immediate_incoming_queue.push(pending_task);
  TraceQueueSize(true);
}

void TaskQueueImpl::ScheduleDelayedWorkTask(TimeDomain* time_domain,
                                            base::TimeTicks desired_run_time) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  LazyNow lazy_now(time_domain->CreateLazyNow());
  time_domain->ScheduleDelayedWork(this, desired_run_time, &lazy_now);
}

bool TaskQueueImpl::IsQueueEnabled() const {
  if (!main_thread_only().task_queue_manager)
    return false;

  return main_thread_only().task_queue_manager->selector_.IsQueueEnabled(this);
}

bool TaskQueueImpl::IsEmpty() const {
  if (!main_thread_only().delayed_work_queue.empty() ||
      !main_thread_only().immediate_work_queue.empty()) {
    return false;
  }

  base::AutoLock lock(any_thread_lock_);
  return any_thread().immediate_incoming_queue.empty() &&
         any_thread().delayed_incoming_queue.empty();
}

bool TaskQueueImpl::HasPendingImmediateWork() const {
  if (!main_thread_only().delayed_work_queue.empty() ||
      !main_thread_only().immediate_work_queue.empty()) {
    return true;
  }

  return NeedsPumping();
}

bool TaskQueueImpl::NeedsPumping() const {
  if (!main_thread_only().immediate_work_queue.empty())
    return false;

  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().immediate_incoming_queue.empty())
    return true;

  // If there's no immediate Incoming work then we only need pumping if there
  // is a delayed task that should be running now.
  if (any_thread().delayed_incoming_queue.empty())
    return false;

  return any_thread().delayed_incoming_queue.top().delayed_run_time <=
         any_thread().time_domain->CreateLazyNow().Now();
}

bool TaskQueueImpl::TaskIsOlderThanQueuedTasks(const Task* task) {
  // A null task is passed when UpdateQueue is called before any task is run.
  // In this case we don't want to pump an after_wakeup queue, so return true
  // here.
  if (!task)
    return true;

  // Return false if task is newer than the oldest immediate task.
  if (!any_thread().immediate_incoming_queue.empty() &&
      task->enqueue_order() >
          any_thread().immediate_incoming_queue.front().enqueue_order()) {
    return false;
  }

  if (main_thread_only().delayed_work_queue.empty())
    return true;

  return task->enqueue_order() <
         main_thread_only().delayed_work_queue.front().enqueue_order();
}

bool TaskQueueImpl::ShouldAutoPumpQueueLocked(bool should_trigger_wakeup,
                                              const Task* previous_task) {
  if (any_thread().pump_policy == PumpPolicy::MANUAL)
    return false;
  if (any_thread().pump_policy == PumpPolicy::AFTER_WAKEUP &&
      (!should_trigger_wakeup || TaskIsOlderThanQueuedTasks(previous_task)))
    return false;
  return true;
}

void TaskQueueImpl::MoveReadyDelayedTasksToDelayedWorkQueueLocked(
    LazyNow* lazy_now) {
  bool was_empty = main_thread_only().delayed_work_queue.empty();

  // Enqueue all delayed tasks that should be running now.
  while (!any_thread().delayed_incoming_queue.empty() &&
         any_thread().delayed_incoming_queue.top().delayed_run_time <=
             lazy_now->Now()) {
    main_thread_only().delayed_work_queue.push(
        std::move(any_thread().delayed_incoming_queue.top()));
    any_thread().delayed_incoming_queue.pop();

    main_thread_only().delayed_work_queue.back().set_enqueue_order(
        any_thread().task_queue_manager->GetNextSequenceNumber());
  }

  if (was_empty && !main_thread_only().delayed_work_queue.empty()) {
    TaskQueueSelector* selector = &any_thread().task_queue_manager->selector_;
    selector->delayed_task_queue_sets()->OnPushQueue(this);
  }
}

void TaskQueueImpl::UpdateDelayedWorkQueue(LazyNow* lazy_now,
                                           bool should_trigger_wakeup,
                                           const Task* previous_task) {
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return;
  if (!ShouldAutoPumpQueueLocked(should_trigger_wakeup, previous_task))
    return;
  MoveReadyDelayedTasksToDelayedWorkQueueLocked(lazy_now);
  TraceQueueSize(true);
}

void TaskQueueImpl::UpdateImmediateWorkQueue(bool should_trigger_wakeup,
                                             const Task* previous_task) {
  DCHECK(main_thread_only().immediate_work_queue.empty());
  base::AutoLock lock(any_thread_lock_);
  if (!any_thread().task_queue_manager)
    return;
  if (!ShouldAutoPumpQueueLocked(should_trigger_wakeup, previous_task))
    return;

  std::swap(main_thread_only().immediate_work_queue,
            any_thread().immediate_incoming_queue);
  // |any_thread().immediate_incoming_queue| is now empty so
  // TimeDomain::UpdateQueues no longer needs to consider this queue for
  // reloading.
  any_thread().time_domain->UnregisterAsUpdatableTaskQueue(this);

  if (!main_thread_only().immediate_work_queue.empty()) {
    TaskQueueSelector* selector =
        &main_thread_only().task_queue_manager->selector_;
    selector->immediate_task_queue_sets()->OnPushQueue(this);
  }
  TraceQueueSize(true);
}

bool TaskQueueImpl::GetDelayedWorkQueueFrontTaskEnqueueOrder(
    int* enqueue_order) const {
  if (main_thread_only().delayed_work_queue.empty())
    return false;
  *enqueue_order =
      main_thread_only().delayed_work_queue.front().enqueue_order();
  return true;
}

bool TaskQueueImpl::GetImmediateWorkQueueFrontTaskEnqueueOrder(
    int* enqueue_order) const {
  if (main_thread_only().immediate_work_queue.empty())
    return false;
  *enqueue_order =
      main_thread_only().immediate_work_queue.front().enqueue_order();
  return true;
}

TaskQueueImpl::Task TaskQueueImpl::TakeTaskFromDelayedWorkQueue() {
  DCHECK(!main_thread_only().delayed_work_queue.empty());
  Task pending_task = std::move(main_thread_only().delayed_work_queue.front());
  main_thread_only().delayed_work_queue.pop();
  DCHECK(main_thread_only().task_queue_manager);
  TaskQueueSelector* selector =
      &main_thread_only().task_queue_manager->selector_;
  selector->delayed_task_queue_sets()->OnPopQueue(this);
  TraceQueueSize(false);
  return pending_task;
}

TaskQueueImpl::Task TaskQueueImpl::TakeTaskFromImmediateWorkQueue() {
  DCHECK(!main_thread_only().immediate_work_queue.empty());
  Task pending_task = main_thread_only().immediate_work_queue.front();
  main_thread_only().immediate_work_queue.pop();
  DCHECK(main_thread_only().task_queue_manager);
  TaskQueueSelector* selector =
      &main_thread_only().task_queue_manager->selector_;
  selector->immediate_task_queue_sets()->OnPopQueue(this);
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
                 any_thread().immediate_incoming_queue.size() +
                     main_thread_only().immediate_work_queue.size() +
                     main_thread_only().delayed_work_queue.size() +
                     any_thread().delayed_incoming_queue.size());
  if (!is_locked)
    any_thread_lock_.Release();
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
  TaskQueueManager* task_queue_manager = any_thread().task_queue_manager;
  if (!task_queue_manager)
    return;

  LazyNow lazy_now(any_thread().time_domain->CreateLazyNow());
  MoveReadyDelayedTasksToDelayedWorkQueueLocked(&lazy_now);

  bool was_empty = main_thread_only().immediate_work_queue.empty();
  while (!any_thread().immediate_incoming_queue.empty()) {
    main_thread_only().immediate_work_queue.push(
        std::move(any_thread().immediate_incoming_queue.front()));
    any_thread().immediate_incoming_queue.pop();
  }

  // |immediate_incoming_queue| is now empty so TimeDomain::UpdateQueues no
  // longer needs to consider this queue for reloading.
  any_thread().time_domain->UnregisterAsUpdatableTaskQueue(this);

  if (main_thread_only().immediate_work_queue.empty() &&
      main_thread_only().delayed_work_queue.empty()) {
    return;
  }

  if (was_empty && !main_thread_only().immediate_work_queue.empty()) {
    task_queue_manager->selector_.immediate_task_queue_sets()->OnPushQueue(
        this);
  }
  task_queue_manager->MaybePostDoWorkOnMainRunner();
}

void TaskQueueImpl::PumpQueue() {
  base::AutoLock lock(any_thread_lock_);
  PumpQueueLocked();
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

void TaskQueueImpl::PushTaskOntoImmediateWorkQueueForTest(const Task& task) {
  main_thread_only().immediate_work_queue.push(task);
}

void TaskQueueImpl::PopTaskFromImmediateWorkQueueForTest() {
  main_thread_only().immediate_work_queue.pop();
}

void TaskQueueImpl::PushTaskOntoDelayedWorkQueueForTest(const Task& task) {
  main_thread_only().delayed_work_queue.push(task);
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
  state->SetString("time_domain_name", any_thread().time_domain->GetName());
  state->SetString("pump_policy", PumpPolicyToString(any_thread().pump_policy));
  state->SetString("wakeup_policy", WakeupPolicyToString(wakeup_policy_));
  bool verbose_tracing_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      disabled_by_default_verbose_tracing_category_, &verbose_tracing_enabled);
  state->SetInteger("immediate_incoming_queue_size",
                    any_thread().immediate_incoming_queue.size());
  state->SetInteger("work_queue_size",
                    main_thread_only().immediate_work_queue.size());
  state->SetInteger("delayed_incoming_queue_size",
                    any_thread().delayed_incoming_queue.size());
  if (verbose_tracing_enabled) {
    state->BeginArray("immediate_incoming_queue");
    QueueAsValueInto(any_thread().immediate_incoming_queue, state);
    state->EndArray();
    state->BeginArray("delayed_work_queue");
    QueueAsValueInto(main_thread_only().delayed_work_queue, state);
    state->EndArray();
    state->BeginArray("immediate_work_queue");
    QueueAsValueInto(main_thread_only().immediate_work_queue, state);
    state->EndArray();
    state->BeginArray("delayed_incoming_queue");
    QueueAsValueInto(any_thread().delayed_incoming_queue, state);
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

void TaskQueueImpl::SetTimeDomain(TimeDomain* time_domain) {
  base::AutoLock lock(any_thread_lock_);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (time_domain == any_thread().time_domain)
    return;

  if (time_domain)
    any_thread().time_domain->MigrateQueue(this, time_domain);
  any_thread().time_domain = time_domain;
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
