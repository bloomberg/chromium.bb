// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_impl.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/time/time.h"
#include "base/trace_event/blame_context.h"
#include "build/build_config.h"

namespace base {
namespace sequence_manager {

// static
const char* TaskQueue::PriorityToString(TaskQueue::QueuePriority priority) {
  switch (priority) {
    case kControlPriority:
      return "control";
    case kHighestPriority:
      return "highest";
    case kHighPriority:
      return "high";
    case kNormalPriority:
      return "normal";
    case kLowPriority:
      return "low";
    case kBestEffortPriority:
      return "best_effort";
    default:
      NOTREACHED();
      return nullptr;
  }
}

namespace internal {

TaskQueueImpl::GuardedTaskPoster::GuardedTaskPoster(TaskQueueImpl* outer)
    : outer_(outer) {}

TaskQueueImpl::GuardedTaskPoster::~GuardedTaskPoster() {}

bool TaskQueueImpl::GuardedTaskPoster::PostTask(PostedTask task) {
  auto token = operations_controller_.TryBeginOperation();
  if (!token)
    return false;

  outer_->PostTask(std::move(task));
  return true;
}

TaskQueueImpl::TaskRunner::TaskRunner(
    scoped_refptr<GuardedTaskPoster> task_poster,
    scoped_refptr<AssociatedThreadId> associated_thread,
    int task_type)
    : task_poster_(std::move(task_poster)),
      associated_thread_(std::move(associated_thread)),
      task_type_(task_type) {}

TaskQueueImpl::TaskRunner::~TaskRunner() {}

bool TaskQueueImpl::TaskRunner::PostDelayedTask(const Location& location,
                                                OnceClosure callback,
                                                TimeDelta delay) {
  return task_poster_->PostTask(PostedTask(std::move(callback), location, delay,
                                           Nestable::kNestable, task_type_));
}

bool TaskQueueImpl::TaskRunner::PostNonNestableDelayedTask(
    const Location& location,
    OnceClosure callback,
    TimeDelta delay) {
  return task_poster_->PostTask(PostedTask(std::move(callback), location, delay,
                                           Nestable::kNonNestable, task_type_));
}

bool TaskQueueImpl::TaskRunner::RunsTasksInCurrentSequence() const {
  return associated_thread_->IsBoundToCurrentThread();
}

TaskQueueImpl::TaskQueueImpl(SequenceManagerImpl* sequence_manager,
                             TimeDomain* time_domain,
                             const TaskQueue::Spec& spec)
    : name_(spec.name),
      sequence_manager_(sequence_manager),
      associated_thread_(sequence_manager
                             ? sequence_manager->associated_thread()
                             : AssociatedThreadId::CreateBound()),
      task_poster_(MakeRefCounted<GuardedTaskPoster>(this)),
      any_thread_(time_domain),
      main_thread_only_(this, time_domain),
      should_monitor_quiescence_(spec.should_monitor_quiescence),
      should_notify_observers_(spec.should_notify_observers),
      delayed_fence_allowed_(spec.delayed_fence_allowed) {
  DCHECK(time_domain);
  // SequenceManager can't be set later, so we need to prevent task runners
  // from posting any tasks.
  if (sequence_manager_)
    task_poster_->StartAcceptingOperations();
}

TaskQueueImpl::~TaskQueueImpl() {
#if DCHECK_IS_ON()
  AutoLock lock(any_thread_lock_);
  // NOTE this check shouldn't fire because |SequenceManagerImpl::queues_|
  // contains a strong reference to this TaskQueueImpl and the
  // SequenceManagerImpl destructor calls UnregisterTaskQueue on all task
  // queues.
  DCHECK(any_thread().unregistered)
      << "UnregisterTaskQueue must be called first!";
#endif
}

TaskQueueImpl::AnyThread::AnyThread(TimeDomain* time_domain)
    : time_domain(time_domain) {}

TaskQueueImpl::AnyThread::~AnyThread() = default;

TaskQueueImpl::MainThreadOnly::MainThreadOnly(TaskQueueImpl* task_queue,
                                              TimeDomain* time_domain)
    : time_domain(time_domain),
      delayed_work_queue(
          new WorkQueue(task_queue, "delayed", WorkQueue::QueueType::kDelayed)),
      immediate_work_queue(new WorkQueue(task_queue,
                                         "immediate",
                                         WorkQueue::QueueType::kImmediate)),
      is_enabled_refcount(0),
      voter_refcount(0),
      blame_context(nullptr),
      is_enabled_for_test(true) {}

TaskQueueImpl::MainThreadOnly::~MainThreadOnly() = default;

scoped_refptr<SingleThreadTaskRunner> TaskQueueImpl::CreateTaskRunner(
    int task_type) const {
  return MakeRefCounted<TaskRunner>(task_poster_, associated_thread_,
                                    task_type);
}

void TaskQueueImpl::UnregisterTaskQueue() {
  // Detach task runners.
  task_poster_->ShutdownAndWaitForZeroOperations();

  TaskDeque immediate_incoming_queue;

  {
    AutoLock lock(any_thread_lock_);
    AutoLock immediate_incoming_queue_lock(immediate_incoming_queue_lock_);

    if (main_thread_only().time_domain)
      main_thread_only().time_domain->UnregisterQueue(this);

    any_thread().unregistered = true;

    main_thread_only().on_task_completed_handler = OnTaskCompletedHandler();
    any_thread().time_domain = nullptr;
    main_thread_only().time_domain = nullptr;

    main_thread_only().on_next_wake_up_changed_callback =
        OnNextWakeUpChangedCallback();
    immediate_incoming_queue.swap(immediate_incoming_queue_);
  }

  // It is possible for a task to hold a scoped_refptr to this, which
  // will lead to TaskQueueImpl destructor being called when deleting a task.
  // To avoid use-after-free, we need to clear all fields of a task queue
  // before starting to delete the tasks.
  // All work queues and priority queues containing tasks should be moved to
  // local variables on stack (std::move for unique_ptrs and swap for queues)
  // before clearing them and deleting tasks.

  // Flush the queues outside of the lock because TSAN complains about a lock
  // order inversion for tasks that are posted from within a lock, with a
  // destructor that acquires the same lock.

  DelayedIncomingQueue delayed_incoming_queue;
  delayed_incoming_queue.swap(&main_thread_only().delayed_incoming_queue);
  std::unique_ptr<WorkQueue> immediate_work_queue =
      std::move(main_thread_only().immediate_work_queue);
  std::unique_ptr<WorkQueue> delayed_work_queue =
      std::move(main_thread_only().delayed_work_queue);
}

const char* TaskQueueImpl::GetName() const {
  return name_;
}

void TaskQueueImpl::PostTask(PostedTask task) {
  CurrentThread current_thread =
      associated_thread_->IsBoundToCurrentThread()
          ? TaskQueueImpl::CurrentThread::kMainThread
          : TaskQueueImpl::CurrentThread::kNotMainThread;

  if (task.delay.is_zero()) {
    PostImmediateTaskImpl(std::move(task), current_thread);
  } else {
    PostDelayedTaskImpl(std::move(task), current_thread);
  }
}

void TaskQueueImpl::PostImmediateTaskImpl(PostedTask task,
                                          CurrentThread current_thread) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.callback);

  TimeTicks now;
  bool add_queue_time_to_tasks = sequence_manager_->GetAddQueueTimeToTasks();
  if (delayed_fence_allowed_ || add_queue_time_to_tasks) {
    if (current_thread == CurrentThread::kMainThread) {
      now = main_thread_only().time_domain->Now();
    } else {
      AutoLock lock(any_thread_lock_);
      now = any_thread().time_domain->Now();
    }
    if (add_queue_time_to_tasks) {
      task.queue_time = now;
    }
  }

  // If the |immediate_incoming_queue| is empty we need a DoWork posted to make
  // it run.
  bool was_immediate_incoming_queue_empty;
  EnqueueOrder sequence_number;
  {
    // TODO(alexclarke): Maybe add a main thread only immediate_incoming_queue
    // See https://crbug.com/901800
    AutoLock lock(immediate_incoming_queue_lock_);
    // The sequence number must be incremented atomically with pushing onto the
    // incoming queue. Otherwise if there are several threads posting task we
    // risk breaking the assumption that sequence numbers increase monotonically
    // within a queue.
    sequence_number = sequence_manager_->GetNextSequenceNumber();
    was_immediate_incoming_queue_empty = immediate_incoming_queue().empty();
    immediate_incoming_queue().push_back(
        Task(std::move(task), now, sequence_number, sequence_number));
    sequence_manager_->WillQueueTask(&immediate_incoming_queue().back());
  }

  if (was_immediate_incoming_queue_empty) {
    bool schedule_work = true;
    // In general we need to ScheduleWork unless the queue is blocked or
    // disabled, however we can only detect that on the main thread. If there's
    // a wake up observer we have to ScheduleWork or we can't guarantee the
    // observer will fire.
    if (current_thread == CurrentThread::kMainThread &&
        main_thread_only().on_next_wake_up_changed_callback.is_null()) {
      schedule_work = IsQueueEnabled() && !main_thread_only().current_fence;
    }
    sequence_manager_->OnQueueHasIncomingImmediateWork(this, sequence_number,
                                                       schedule_work);
  }

  TraceQueueSize();
}

void TaskQueueImpl::PostDelayedTaskImpl(PostedTask task,
                                        CurrentThread current_thread) {
  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task.callback);
  DCHECK_GT(task.delay, TimeDelta());

  WakeUpResolution resolution = WakeUpResolution::kLow;
#if defined(OS_WIN)
  // We consider the task needs a high resolution timer if the delay is more
  // than 0 and less than 32ms. This caps the relative error to less than 50% :
  // a 33ms wait can wake at 48ms since the default resolution on Windows is
  // between 10 and 15ms.
  if (task.delay.InMilliseconds() < (2 * Time::kMinLowResolutionThresholdMs))
    resolution = WakeUpResolution::kHigh;
#endif  // defined(OS_WIN)

  if (current_thread == CurrentThread::kMainThread) {
    // Lock-free fast path for delayed tasks posted from the main thread.
    EnqueueOrder sequence_number = sequence_manager_->GetNextSequenceNumber();

    TimeTicks time_domain_now = main_thread_only().time_domain->Now();
    TimeTicks time_domain_delayed_run_time = time_domain_now + task.delay;
    if (sequence_manager_->GetAddQueueTimeToTasks()) {
      task.queue_time = time_domain_now;
    }

    PushOntoDelayedIncomingQueueFromMainThread(
        Task(std::move(task), time_domain_delayed_run_time, sequence_number,
             EnqueueOrder(), resolution),
        time_domain_now, /* notify_task_annotator */ true);
  } else {
    // NOTE posting a delayed task from a different thread is not expected to
    // be common. This pathway is less optimal than perhaps it could be
    // because it causes two main thread tasks to be run.  Should this
    // assumption prove to be false in future, we may need to revisit this.
    EnqueueOrder sequence_number = sequence_manager_->GetNextSequenceNumber();

    TimeTicks time_domain_now;
    {
      AutoLock lock(any_thread_lock_);
      time_domain_now = any_thread().time_domain->Now();
    }
    TimeTicks time_domain_delayed_run_time = time_domain_now + task.delay;
    if (sequence_manager_->GetAddQueueTimeToTasks()) {
      task.queue_time = time_domain_now;
    }

    PushOntoDelayedIncomingQueue(
        Task(std::move(task), time_domain_delayed_run_time, sequence_number,
             EnqueueOrder(), resolution));
  }
}

void TaskQueueImpl::PushOntoDelayedIncomingQueueFromMainThread(
    Task pending_task,
    TimeTicks now,
    bool notify_task_annotator) {
  if (notify_task_annotator)
    sequence_manager_->WillQueueTask(&pending_task);
  main_thread_only().delayed_incoming_queue.push(std::move(pending_task));

  LazyNow lazy_now(now);
  UpdateDelayedWakeUp(&lazy_now);

  TraceQueueSize();
}

void TaskQueueImpl::PushOntoDelayedIncomingQueue(Task pending_task) {
  sequence_manager_->WillQueueTask(&pending_task);

  // TODO(altimin): Add a copy method to Task to capture metadata here.
  PostImmediateTaskImpl(
      PostedTask(BindOnce(&TaskQueueImpl::ScheduleDelayedWorkTask,
                          Unretained(this), std::move(pending_task)),
                 FROM_HERE, TimeDelta(), Nestable::kNonNestable,
                 pending_task.task_type),
      CurrentThread::kNotMainThread);
}

void TaskQueueImpl::ScheduleDelayedWorkTask(Task pending_task) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  TimeTicks delayed_run_time = pending_task.delayed_run_time;
  TimeTicks time_domain_now = main_thread_only().time_domain->Now();
  if (delayed_run_time <= time_domain_now) {
    // If |delayed_run_time| is in the past then push it onto the work queue
    // immediately. To ensure the right task ordering we need to temporarily
    // push it onto the |delayed_incoming_queue|.
    delayed_run_time = time_domain_now;
    pending_task.delayed_run_time = time_domain_now;
    main_thread_only().delayed_incoming_queue.push(std::move(pending_task));
    LazyNow lazy_now(time_domain_now);
    WakeUpForDelayedWork(&lazy_now);
  } else {
    // If |delayed_run_time| is in the future we can queue it as normal.
    PushOntoDelayedIncomingQueueFromMainThread(std::move(pending_task),
                                               time_domain_now, false);
  }
  TraceQueueSize();
}

void TaskQueueImpl::ReloadImmediateWorkQueueIfEmpty() {
  if (!main_thread_only().immediate_work_queue->Empty())
    return;

  main_thread_only().immediate_work_queue->ReloadEmptyImmediateQueue();

  if (!main_thread_only().on_next_wake_up_changed_callback.is_null() &&
      IsQueueEnabled()) {
    main_thread_only().on_next_wake_up_changed_callback.Run(TimeTicks());
  }
}

void TaskQueueImpl::ReloadEmptyImmediateQueue(TaskDeque* queue) {
  DCHECK(queue->empty());

  {
    AutoLock immediate_incoming_queue_lock(immediate_incoming_queue_lock_);
    queue->swap(immediate_incoming_queue());

    // Since |immediate_incoming_queue| is empty, now is a good time to consider
    // reducing it's capacity if we're wasting memory.
    immediate_incoming_queue().MaybeShrinkQueue();
  }

  // Activate delayed fence if necessary. This is ideologically similar to
  // ActivateDelayedFenceIfNeeded, but due to immediate tasks being posted
  // from any thread we can't generate an enqueue order for the fence there,
  // so we have to check all immediate tasks and use their enqueue order for
  // a fence.
  if (main_thread_only().delayed_fence) {
    for (const Task& task : *queue) {
      if (task.delayed_run_time >= main_thread_only().delayed_fence.value()) {
        main_thread_only().delayed_fence = nullopt;
        DCHECK(!main_thread_only().current_fence);
        main_thread_only().current_fence = task.enqueue_order();
        // Do not trigger WorkQueueSets notification when taking incoming
        // immediate queue.
        main_thread_only().immediate_work_queue->InsertFenceSilently(
            main_thread_only().current_fence);
        main_thread_only().delayed_work_queue->InsertFenceSilently(
            main_thread_only().current_fence);
        break;
      }
    }
  }
}

bool TaskQueueImpl::IsEmpty() const {
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().delayed_incoming_queue.empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return false;
  }

  AutoLock lock(immediate_incoming_queue_lock_);
  return immediate_incoming_queue().empty();
}

size_t TaskQueueImpl::GetNumberOfPendingTasks() const {
  size_t task_count = 0;
  task_count += main_thread_only().delayed_work_queue->Size();
  task_count += main_thread_only().delayed_incoming_queue.size();
  task_count += main_thread_only().immediate_work_queue->Size();

  AutoLock lock(immediate_incoming_queue_lock_);
  task_count += immediate_incoming_queue().size();
  return task_count;
}

bool TaskQueueImpl::HasTaskToRunImmediately() const {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Tasks on |delayed_incoming_queue| that could run now, count as
  // immediate work.
  if (!main_thread_only().delayed_incoming_queue.empty() &&
      main_thread_only().delayed_incoming_queue.top().delayed_run_time <=
          main_thread_only().time_domain->CreateLazyNow().Now()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  AutoLock lock(immediate_incoming_queue_lock_);
  return !immediate_incoming_queue().empty();
}

Optional<DelayedWakeUp> TaskQueueImpl::GetNextScheduledWakeUpImpl() {
  // Note we don't scheduled a wake-up for disabled queues.
  if (main_thread_only().delayed_incoming_queue.empty() || !IsQueueEnabled())
    return nullopt;

  return main_thread_only().delayed_incoming_queue.top().delayed_wake_up();
}

Optional<TimeTicks> TaskQueueImpl::GetNextScheduledWakeUp() {
  Optional<DelayedWakeUp> wake_up = GetNextScheduledWakeUpImpl();
  if (!wake_up)
    return nullopt;
  return wake_up->time;
}

void TaskQueueImpl::WakeUpForDelayedWork(LazyNow* lazy_now) {
  // Enqueue all delayed tasks that should be running now, skipping any that
  // have been canceled.
  while (!main_thread_only().delayed_incoming_queue.empty()) {
    Task& task =
        const_cast<Task&>(main_thread_only().delayed_incoming_queue.top());
    if (!task.task || task.task.IsCancelled()) {
      main_thread_only().delayed_incoming_queue.pop();
      continue;
    }
    if (task.delayed_run_time > lazy_now->Now())
      break;
    ActivateDelayedFenceIfNeeded(task.delayed_run_time);
    DCHECK(!task.enqueue_order_set());
    task.set_enqueue_order(sequence_manager_->GetNextSequenceNumber());
    main_thread_only().delayed_work_queue->Push(std::move(task));
    main_thread_only().delayed_incoming_queue.pop();

    // Normally WakeUpForDelayedWork is called inside DoWork, but it also
    // can be called elsewhere (e.g. tests and fast-path for posting
    // delayed tasks). Ensure that there is a DoWork posting. No-op inside
    // existing DoWork due to DoWork deduplication.
    if (IsQueueEnabled() || !main_thread_only().current_fence) {
      sequence_manager_->MaybeScheduleImmediateWork(FROM_HERE);
    }
  }

  UpdateDelayedWakeUp(lazy_now);
}

void TaskQueueImpl::TraceQueueSize() const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager"), &is_tracing);
  if (!is_tracing)
    return;

  // It's only safe to access the work queues from the main thread.
  // TODO(alexclarke): We should find another way of tracing this
  if (!associated_thread_->IsBoundToCurrentThread())
    return;

  AutoLock lock(immediate_incoming_queue_lock_);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("sequence_manager"), GetName(),
                 immediate_incoming_queue().size() +
                     main_thread_only().immediate_work_queue->Size() +
                     main_thread_only().delayed_work_queue->Size() +
                     main_thread_only().delayed_incoming_queue.size());
}

void TaskQueueImpl::SetQueuePriority(TaskQueue::QueuePriority priority) {
  if (priority == GetQueuePriority())
    return;
  sequence_manager_->main_thread_only().selector.SetQueuePriority(this,
                                                                  priority);
}

TaskQueue::QueuePriority TaskQueueImpl::GetQueuePriority() const {
  size_t set_index = immediate_work_queue()->work_queue_set_index();
  DCHECK_EQ(set_index, delayed_work_queue()->work_queue_set_index());
  return static_cast<TaskQueue::QueuePriority>(set_index);
}

void TaskQueueImpl::AsValueInto(TimeTicks now,
                                trace_event::TracedValue* state,
                                bool force_verbose) const {
  AutoLock lock(any_thread_lock_);
  AutoLock immediate_incoming_queue_lock(immediate_incoming_queue_lock_);
  state->BeginDictionary();
  state->SetString("name", GetName());
  if (any_thread().unregistered) {
    state->SetBoolean("unregistered", true);
    state->EndDictionary();
    return;
  }
  DCHECK(main_thread_only().time_domain);
  DCHECK(main_thread_only().delayed_work_queue);
  DCHECK(main_thread_only().immediate_work_queue);

  state->SetString(
      "task_queue_id",
      StringPrintf("0x%" PRIx64,
                   static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this))));
  state->SetBoolean("enabled", IsQueueEnabled());
  state->SetString("time_domain_name",
                   main_thread_only().time_domain->GetName());
  state->SetInteger("immediate_incoming_queue_size",
                    immediate_incoming_queue().size());
  state->SetInteger("delayed_incoming_queue_size",
                    main_thread_only().delayed_incoming_queue.size());
  state->SetInteger("immediate_work_queue_size",
                    main_thread_only().immediate_work_queue->Size());
  state->SetInteger("delayed_work_queue_size",
                    main_thread_only().delayed_work_queue->Size());

  state->SetInteger("immediate_incoming_queue_capacity",
                    immediate_incoming_queue().capacity());
  state->SetInteger("immediate_work_queue_capacity",
                    immediate_work_queue()->Capacity());
  state->SetInteger("delayed_work_queue_capacity",
                    delayed_work_queue()->Capacity());

  if (!main_thread_only().delayed_incoming_queue.empty()) {
    TimeDelta delay_to_next_task =
        (main_thread_only().delayed_incoming_queue.top().delayed_run_time -
         main_thread_only().time_domain->CreateLazyNow().Now());
    state->SetDouble("delay_to_next_task_ms",
                     delay_to_next_task.InMillisecondsF());
  }
  if (main_thread_only().current_fence)
    state->SetInteger("current_fence", main_thread_only().current_fence);
  if (main_thread_only().delayed_fence) {
    state->SetDouble(
        "delayed_fence_seconds_from_now",
        (main_thread_only().delayed_fence.value() - now).InSecondsF());
  }

  bool verbose = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("sequence_manager.verbose_snapshots"),
      &verbose);

  if (verbose || force_verbose) {
    state->BeginArray("immediate_incoming_queue");
    QueueAsValueInto(immediate_incoming_queue(), now, state);
    state->EndArray();
    state->BeginArray("delayed_work_queue");
    main_thread_only().delayed_work_queue->AsValueInto(now, state);
    state->EndArray();
    state->BeginArray("immediate_work_queue");
    main_thread_only().immediate_work_queue->AsValueInto(now, state);
    state->EndArray();
    state->BeginArray("delayed_incoming_queue");
    main_thread_only().delayed_incoming_queue.AsValueInto(now, state);
    state->EndArray();
  }
  state->SetString("priority", TaskQueue::PriorityToString(GetQueuePriority()));
  state->EndDictionary();
}

void TaskQueueImpl::AddTaskObserver(MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.AddObserver(task_observer);
}

void TaskQueueImpl::RemoveTaskObserver(
    MessageLoop::TaskObserver* task_observer) {
  main_thread_only().task_observers.RemoveObserver(task_observer);
}

void TaskQueueImpl::NotifyWillProcessTask(const PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Enter();
  for (auto& observer : main_thread_only().task_observers)
    observer.WillProcessTask(pending_task);
}

void TaskQueueImpl::NotifyDidProcessTask(const PendingTask& pending_task) {
  DCHECK(should_notify_observers_);
  for (auto& observer : main_thread_only().task_observers)
    observer.DidProcessTask(pending_task);
  if (main_thread_only().blame_context)
    main_thread_only().blame_context->Leave();
}

void TaskQueueImpl::SetTimeDomain(TimeDomain* time_domain) {
  {
    AutoLock lock(any_thread_lock_);
    DCHECK(time_domain);
    DCHECK(!any_thread().unregistered);
    if (any_thread().unregistered)
      return;
    DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
    if (time_domain == main_thread_only().time_domain)
      return;

    any_thread().time_domain = time_domain;
  }

  main_thread_only().time_domain->UnregisterQueue(this);
  main_thread_only().time_domain = time_domain;

  LazyNow lazy_now = time_domain->CreateLazyNow();
  // Clear scheduled wake up to ensure that new notifications are issued
  // correctly.
  // TODO(altimin): Remove this when we won't have to support changing time
  // domains.
  main_thread_only().scheduled_wake_up = nullopt;
  UpdateDelayedWakeUp(&lazy_now);
}

TimeDomain* TaskQueueImpl::GetTimeDomain() const {
  DCHECK(associated_thread_->IsBoundToCurrentThread() ||
         !associated_thread_->IsBound());
  return main_thread_only().time_domain;
}

void TaskQueueImpl::SetBlameContext(trace_event::BlameContext* blame_context) {
  main_thread_only().blame_context = blame_context;
}

void TaskQueueImpl::InsertFence(TaskQueue::InsertFencePosition position) {
  // Only one fence may be present at a time.
  main_thread_only().delayed_fence = nullopt;

  EnqueueOrder previous_fence = main_thread_only().current_fence;
  EnqueueOrder current_fence = position == TaskQueue::InsertFencePosition::kNow
                                   ? sequence_manager_->GetNextSequenceNumber()
                                   : EnqueueOrder::blocking_fence();

  // Tasks posted after this point will have a strictly higher enqueue order
  // and will be blocked from running.
  main_thread_only().current_fence = current_fence;
  bool task_unblocked =
      main_thread_only().immediate_work_queue->InsertFence(current_fence);
  task_unblocked |=
      main_thread_only().delayed_work_queue->InsertFence(current_fence);

  if (!task_unblocked && previous_fence && previous_fence < current_fence) {
    AutoLock lock(immediate_incoming_queue_lock_);
    if (!immediate_incoming_queue().empty() &&
        immediate_incoming_queue().front().enqueue_order() > previous_fence &&
        immediate_incoming_queue().front().enqueue_order() < current_fence) {
      task_unblocked = true;
    }
  }

  if (IsQueueEnabled() && task_unblocked)
    sequence_manager_->MaybeScheduleImmediateWork(FROM_HERE);
}

void TaskQueueImpl::InsertFenceAt(TimeTicks time) {
  DCHECK(delayed_fence_allowed_)
      << "Delayed fences are not supported for this queue. Enable them "
         "explicitly in TaskQueue::Spec when creating the queue";

  // Task queue can have only one fence, delayed or not.
  RemoveFence();
  main_thread_only().delayed_fence = time;
}

void TaskQueueImpl::RemoveFence() {
  EnqueueOrder previous_fence = main_thread_only().current_fence;
  main_thread_only().current_fence = EnqueueOrder::none();
  main_thread_only().delayed_fence = nullopt;

  bool task_unblocked = main_thread_only().immediate_work_queue->RemoveFence();
  task_unblocked |= main_thread_only().delayed_work_queue->RemoveFence();

  if (!task_unblocked && previous_fence) {
    AutoLock lock(immediate_incoming_queue_lock_);
    if (!immediate_incoming_queue().empty() &&
        immediate_incoming_queue().front().enqueue_order() > previous_fence) {
      task_unblocked = true;
    }
  }

  if (IsQueueEnabled() && task_unblocked)
    sequence_manager_->MaybeScheduleImmediateWork(FROM_HERE);
}

bool TaskQueueImpl::BlockedByFence() const {
  if (!main_thread_only().current_fence)
    return false;

  if (!main_thread_only().immediate_work_queue->BlockedByFence() ||
      !main_thread_only().delayed_work_queue->BlockedByFence()) {
    return false;
  }

  AutoLock lock(immediate_incoming_queue_lock_);
  if (immediate_incoming_queue().empty())
    return true;

  return immediate_incoming_queue().front().enqueue_order() >
         main_thread_only().current_fence;
}

bool TaskQueueImpl::HasActiveFence() {
  if (main_thread_only().delayed_fence &&
      main_thread_only().time_domain->Now() >
          main_thread_only().delayed_fence.value()) {
    return true;
  }
  return !!main_thread_only().current_fence;
}

bool TaskQueueImpl::CouldTaskRun(EnqueueOrder enqueue_order) const {
  if (!IsQueueEnabled())
    return false;

  if (!main_thread_only().current_fence)
    return true;

  return enqueue_order < main_thread_only().current_fence;
}

// static
void TaskQueueImpl::QueueAsValueInto(const TaskDeque& queue,
                                     TimeTicks now,
                                     trace_event::TracedValue* state) {
  for (const Task& task : queue) {
    TaskAsValueInto(task, now, state);
  }
}

// static
void TaskQueueImpl::TaskAsValueInto(const Task& task,
                                    TimeTicks now,
                                    trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("posted_from", task.posted_from.ToString());
  if (task.enqueue_order_set())
    state->SetInteger("enqueue_order", task.enqueue_order());
  state->SetInteger("sequence_num", task.sequence_num);
  state->SetBoolean("nestable", task.nestable == Nestable::kNestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetBoolean("is_cancelled", task.task.IsCancelled());
  state->SetDouble("delayed_run_time",
                   (task.delayed_run_time - TimeTicks()).InMillisecondsF());
  state->SetDouble("delayed_run_time_milliseconds_from_now",
                   (task.delayed_run_time - now).InMillisecondsF());
  state->EndDictionary();
}

TaskQueueImpl::QueueEnabledVoterImpl::QueueEnabledVoterImpl(
    scoped_refptr<TaskQueue> task_queue)
    : task_queue_(task_queue), enabled_(true) {}

TaskQueueImpl::QueueEnabledVoterImpl::~QueueEnabledVoterImpl() {
  if (task_queue_->GetTaskQueueImpl())
    task_queue_->GetTaskQueueImpl()->RemoveQueueEnabledVoter(this);
}

void TaskQueueImpl::QueueEnabledVoterImpl::SetQueueEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  task_queue_->GetTaskQueueImpl()->OnQueueEnabledVoteChanged(enabled);
  enabled_ = enabled;
}

void TaskQueueImpl::RemoveQueueEnabledVoter(
    const QueueEnabledVoterImpl* voter) {
  // Bail out if we're being called from TaskQueueImpl::UnregisterTaskQueue.
  if (!main_thread_only().time_domain)
    return;

  bool was_enabled = IsQueueEnabled();
  if (voter->enabled_) {
    main_thread_only().is_enabled_refcount--;
    DCHECK_GE(main_thread_only().is_enabled_refcount, 0);
  }

  main_thread_only().voter_refcount--;
  DCHECK_GE(main_thread_only().voter_refcount, 0);

  bool is_enabled = IsQueueEnabled();
  if (was_enabled != is_enabled)
    EnableOrDisableWithSelector(is_enabled);
}

bool TaskQueueImpl::IsQueueEnabled() const {
  // By default is_enabled_refcount and voter_refcount both equal zero.
  return (main_thread_only().is_enabled_refcount ==
          main_thread_only().voter_refcount) &&
         main_thread_only().is_enabled_for_test;
}

void TaskQueueImpl::OnQueueEnabledVoteChanged(bool enabled) {
  bool was_enabled = IsQueueEnabled();
  if (enabled) {
    main_thread_only().is_enabled_refcount++;
    DCHECK_LE(main_thread_only().is_enabled_refcount,
              main_thread_only().voter_refcount);
  } else {
    main_thread_only().is_enabled_refcount--;
    DCHECK_GE(main_thread_only().is_enabled_refcount, 0);
  }

  bool is_enabled = IsQueueEnabled();
  if (was_enabled != is_enabled)
    EnableOrDisableWithSelector(is_enabled);
}

void TaskQueueImpl::EnableOrDisableWithSelector(bool enable) {
  // |sequence_manager_| can be null in tests.
  if (!sequence_manager_)
    return;

  LazyNow lazy_now = main_thread_only().time_domain->CreateLazyNow();
  UpdateDelayedWakeUp(&lazy_now);

  if (enable) {
    if (HasPendingImmediateWork() &&
        !main_thread_only().on_next_wake_up_changed_callback.is_null()) {
      // Delayed work notification will be issued via time domain.
      main_thread_only().on_next_wake_up_changed_callback.Run(TimeTicks());
    }

    // Note the selector calls SequenceManager::OnTaskQueueEnabled which posts
    // a DoWork if needed.
    sequence_manager_->main_thread_only().selector.EnableQueue(this);
  } else {
    sequence_manager_->main_thread_only().selector.DisableQueue(this);
  }
}

std::unique_ptr<TaskQueue::QueueEnabledVoter>
TaskQueueImpl::CreateQueueEnabledVoter(scoped_refptr<TaskQueue> task_queue) {
  DCHECK_EQ(task_queue->GetTaskQueueImpl(), this);
  main_thread_only().voter_refcount++;
  main_thread_only().is_enabled_refcount++;
  return std::make_unique<QueueEnabledVoterImpl>(task_queue);
}

void TaskQueueImpl::ReclaimMemory(TimeTicks now) {
  if (main_thread_only().delayed_incoming_queue.empty())
    return;
  main_thread_only().delayed_incoming_queue.SweepCancelledTasks();

  // Also consider shrinking the work queue if it's wasting memory.
  main_thread_only().delayed_work_queue->MaybeShrinkQueue();
  main_thread_only().immediate_work_queue->MaybeShrinkQueue();

  {
    AutoLock lock(immediate_incoming_queue_lock_);
    immediate_incoming_queue().MaybeShrinkQueue();
  }

  LazyNow lazy_now(now);
  UpdateDelayedWakeUp(&lazy_now);
}

void TaskQueueImpl::PushImmediateIncomingTaskForTest(Task&& task) {
  AutoLock lock(immediate_incoming_queue_lock_);
  immediate_incoming_queue().push_back(std::move(task));
}

void TaskQueueImpl::RequeueDeferredNonNestableTask(
    DeferredNonNestableTask task) {
  DCHECK(task.task.nestable == Nestable::kNonNestable);
  // The re-queued tasks have to be pushed onto the front because we'd otherwise
  // violate the strict monotonically increasing enqueue order within the
  // WorkQueue.  We can't assign them a new enqueue order here because that will
  // not behave correctly with fences and things will break (e.g Idle TQ).
  if (task.work_queue_type == WorkQueueType::kDelayed) {
    main_thread_only().delayed_work_queue->PushNonNestableTaskToFront(
        std::move(task.task));
  } else {
    main_thread_only().immediate_work_queue->PushNonNestableTaskToFront(
        std::move(task.task));
  }
}

void TaskQueueImpl::SetOnNextWakeUpChangedCallback(
    TaskQueueImpl::OnNextWakeUpChangedCallback callback) {
#if DCHECK_IS_ON()
  if (callback) {
    DCHECK(main_thread_only().on_next_wake_up_changed_callback.is_null())
        << "Can't assign two different observers to "
           "blink::scheduler::TaskQueue";
  }
#endif
  main_thread_only().on_next_wake_up_changed_callback = callback;
}

void TaskQueueImpl::UpdateDelayedWakeUp(LazyNow* lazy_now) {
  return UpdateDelayedWakeUpImpl(lazy_now, GetNextScheduledWakeUpImpl());
}

void TaskQueueImpl::UpdateDelayedWakeUpImpl(LazyNow* lazy_now,
                                            Optional<DelayedWakeUp> wake_up) {
  if (main_thread_only().scheduled_wake_up == wake_up)
    return;
  main_thread_only().scheduled_wake_up = wake_up;

  if (wake_up &&
      !main_thread_only().on_next_wake_up_changed_callback.is_null() &&
      !HasPendingImmediateWork()) {
    main_thread_only().on_next_wake_up_changed_callback.Run(wake_up->time);
  }

  WakeUpResolution resolution = has_pending_high_resolution_tasks()
                                    ? WakeUpResolution::kHigh
                                    : WakeUpResolution::kLow;
  main_thread_only().time_domain->SetNextWakeUpForQueue(this, wake_up,
                                                        resolution, lazy_now);
}

void TaskQueueImpl::SetDelayedWakeUpForTesting(
    Optional<DelayedWakeUp> wake_up) {
  LazyNow lazy_now = main_thread_only().time_domain->CreateLazyNow();
  UpdateDelayedWakeUpImpl(&lazy_now, wake_up);
}

bool TaskQueueImpl::HasPendingImmediateWork() {
  // Any work queue tasks count as immediate work.
  if (!main_thread_only().delayed_work_queue->Empty() ||
      !main_thread_only().immediate_work_queue->Empty()) {
    return true;
  }

  // Finally tasks on |immediate_incoming_queue| count as immediate work.
  AutoLock lock(immediate_incoming_queue_lock_);
  return !immediate_incoming_queue().empty();
}

void TaskQueueImpl::SetOnTaskStartedHandler(
    TaskQueueImpl::OnTaskStartedHandler handler) {
  DCHECK(should_notify_observers_ || handler.is_null());
  main_thread_only().on_task_started_handler = std::move(handler);
}

void TaskQueueImpl::OnTaskStarted(const Task& task,
                                  const TaskQueue::TaskTiming& task_timing) {
  if (!main_thread_only().on_task_started_handler.is_null())
    main_thread_only().on_task_started_handler.Run(task, task_timing);
}

void TaskQueueImpl::SetOnTaskCompletedHandler(
    TaskQueueImpl::OnTaskCompletedHandler handler) {
  DCHECK(should_notify_observers_ || handler.is_null());
  main_thread_only().on_task_completed_handler = std::move(handler);
}

void TaskQueueImpl::OnTaskCompleted(const Task& task,
                                    const TaskQueue::TaskTiming& task_timing) {
  if (!main_thread_only().on_task_completed_handler.is_null())
    main_thread_only().on_task_completed_handler.Run(task, task_timing);
}

bool TaskQueueImpl::RequiresTaskTiming() const {
  return !main_thread_only().on_task_started_handler.is_null() ||
         !main_thread_only().on_task_completed_handler.is_null();
}

bool TaskQueueImpl::IsUnregistered() const {
  AutoLock lock(any_thread_lock_);
  return any_thread().unregistered;
}

WeakPtr<SequenceManagerImpl> TaskQueueImpl::GetSequenceManagerWeakPtr() {
  return sequence_manager_->GetWeakPtr();
}

void TaskQueueImpl::SetQueueEnabledForTest(bool enabled) {
  main_thread_only().is_enabled_for_test = enabled;
  EnableOrDisableWithSelector(IsQueueEnabled());
}

void TaskQueueImpl::ActivateDelayedFenceIfNeeded(TimeTicks now) {
  if (!main_thread_only().delayed_fence)
    return;
  if (main_thread_only().delayed_fence.value() > now)
    return;
  InsertFence(TaskQueue::InsertFencePosition::kNow);
  main_thread_only().delayed_fence = nullopt;
}

void TaskQueueImpl::DeletePendingTasks() {
  main_thread_only().delayed_work_queue->DeletePendingTasks();
  main_thread_only().immediate_work_queue->DeletePendingTasks();
  // TODO(altimin): Add clear() method to DelayedIncomingQueue.
  DelayedIncomingQueue queue_to_delete;
  main_thread_only().delayed_incoming_queue.swap(&queue_to_delete);

  TaskDeque deque;
  {
    // Limit the scope of the lock to ensure that the deque is destroyed
    // outside of the lock to allow it to post tasks.
    base::AutoLock lock(immediate_incoming_queue_lock_);
    deque.swap(immediate_incoming_queue());
  }

  LazyNow lazy_now = main_thread_only().time_domain->CreateLazyNow();
  UpdateDelayedWakeUp(&lazy_now);
}

bool TaskQueueImpl::HasTasks() const {
  if (!main_thread_only().delayed_work_queue->Empty())
    return true;
  if (!main_thread_only().immediate_work_queue->Empty())
    return true;
  if (!main_thread_only().delayed_incoming_queue.empty())
    return true;

  base::AutoLock lock(immediate_incoming_queue_lock_);
  if (!immediate_incoming_queue().empty())
    return true;

  return false;
}

TaskQueueImpl::DelayedIncomingQueue::DelayedIncomingQueue() = default;
TaskQueueImpl::DelayedIncomingQueue::~DelayedIncomingQueue() = default;

void TaskQueueImpl::DelayedIncomingQueue::push(Task&& task) {
  if (task.is_high_res)
    pending_high_res_tasks_++;
  queue_.push(std::move(task));
}

void TaskQueueImpl::DelayedIncomingQueue::pop() {
  DCHECK(!empty());
  if (top().is_high_res) {
    pending_high_res_tasks_--;
    DCHECK_GE(pending_high_res_tasks_, 0);
  }
  queue_.pop();
}

void TaskQueueImpl::DelayedIncomingQueue::swap(DelayedIncomingQueue* rhs) {
  std::swap(pending_high_res_tasks_, rhs->pending_high_res_tasks_);
  std::swap(queue_, rhs->queue_);
}

void TaskQueueImpl::DelayedIncomingQueue::SweepCancelledTasks() {
  // Under the hood a std::priority_queue is a heap and usually it's built on
  // top of a std::vector. We poke at that vector directly here to filter out
  // canceled tasks in place.
  bool task_deleted = false;
  auto it = queue_.c.begin();
  while (it != queue_.c.end()) {
    if (it->task.IsCancelled()) {
      if (it->is_high_res)
        pending_high_res_tasks_--;
      *it = std::move(queue_.c.back());
      queue_.c.pop_back();
      task_deleted = true;
    } else {
      it++;
    }
  }

  // If we deleted something, re-enforce the heap property.
  if (task_deleted)
    std::make_heap(queue_.c.begin(), queue_.c.end(), queue_.comp);
}

void TaskQueueImpl::DelayedIncomingQueue::AsValueInto(
    TimeTicks now,
    trace_event::TracedValue* state) const {
  for (const Task& task : queue_.c) {
    TaskAsValueInto(task, now, state);
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
