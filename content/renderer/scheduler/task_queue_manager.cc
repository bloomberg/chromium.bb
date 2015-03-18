// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/task_queue_manager.h"

#include <queue>
#include <set>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/test/test_now_source.h"
#include "content/renderer/scheduler/nestable_single_thread_task_runner.h"
#include "content/renderer/scheduler/task_queue_selector.h"

namespace {
const int64_t kMaxTimeTicks = std::numeric_limits<int64>::max();
}

namespace content {
namespace internal {

// Now() is somewhat expensive so it makes sense not to call Now() unless we
// really need to.
class LazyNow {
 public:
  explicit LazyNow(base::TimeTicks now)
      : task_queue_manager_(nullptr), now_(now) {
    DCHECK(!now.is_null());
  }

  explicit LazyNow(TaskQueueManager* task_queue_manager)
      : task_queue_manager_(task_queue_manager) {}

  base::TimeTicks Now() {
    if (now_.is_null())
      now_ = task_queue_manager_->Now();
    return now_;
  }

 private:
  TaskQueueManager* task_queue_manager_;  // NOT OWNED
  base::TimeTicks now_;
};

class TaskQueue : public base::SingleThreadTaskRunner {
 public:
  TaskQueue(TaskQueueManager* task_queue_manager);

  // base::SingleThreadTaskRunner implementation.
  bool RunsTasksOnCurrentThread() const override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return PostDelayedTaskImpl(from_here, task, delay, TaskType::NORMAL);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    return PostDelayedTaskImpl(from_here, task, delay, TaskType::NON_NESTABLE);
  }

  bool IsQueueEmpty() const;

  void SetPumpPolicy(TaskQueueManager::PumpPolicy pump_policy);
  void PumpQueue();

  bool NextPendingDelayedTaskRunTime(
      base::TimeTicks* next_pending_delayed_task);

  bool UpdateWorkQueue(LazyNow* lazy_now,
                       const base::PendingTask* previous_task);
  base::PendingTask TakeTaskFromWorkQueue();

  void WillDeleteTaskQueueManager();

  base::TaskQueue& work_queue() { return work_queue_; }

  void set_name(const char* name) { name_ = name; }

  void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  enum class TaskType {
    NORMAL,
    NON_NESTABLE,
  };

  ~TaskQueue() override;

  bool PostDelayedTaskImpl(const tracked_objects::Location& from_here,
                           const base::Closure& task,
                           base::TimeDelta delay,
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
  bool ShouldAutoPumpQueueLocked(const base::PendingTask* previous_task);
  void EnqueueTaskLocked(const base::PendingTask& pending_task);

  void TraceQueueSize(bool is_locked) const;
  static const char* PumpPolicyToString(
      TaskQueueManager::PumpPolicy pump_policy);
  static void QueueAsValueInto(const base::TaskQueue& queue,
                               base::trace_event::TracedValue* state);
  static void QueueAsValueInto(const base::DelayedTaskQueue& queue,
                               base::trace_event::TracedValue* state);
  static void TaskAsValueInto(const base::PendingTask& task,
                              base::trace_event::TracedValue* state);

  // This lock protects all members except the work queue and the
  // main_thread_checker_.
  mutable base::Lock lock_;
  base::PlatformThreadId thread_id_;
  TaskQueueManager* task_queue_manager_;
  base::TaskQueue incoming_queue_;
  TaskQueueManager::PumpPolicy pump_policy_;
  const char* name_;
  base::DelayedTaskQueue delayed_task_queue_;
  std::set<base::TimeTicks> in_flight_kick_delayed_tasks_;

  base::ThreadChecker main_thread_checker_;
  base::TaskQueue work_queue_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueue);
};

TaskQueue::TaskQueue(TaskQueueManager* task_queue_manager)
    : thread_id_(base::PlatformThread::CurrentId()),
      task_queue_manager_(task_queue_manager),
      pump_policy_(TaskQueueManager::PumpPolicy::AUTO),
      name_(nullptr) {
}

TaskQueue::~TaskQueue() {
}

void TaskQueue::WillDeleteTaskQueueManager() {
  base::AutoLock lock(lock_);
  task_queue_manager_ = nullptr;
  // TODO(scheduler-dev): Should we also clear the other queues here too?
  delayed_task_queue_ = base::DelayedTaskQueue();
}

bool TaskQueue::RunsTasksOnCurrentThread() const {
  base::AutoLock lock(lock_);
  return base::PlatformThread::CurrentId() == thread_id_;
}

bool TaskQueue::PostDelayedTaskImpl(const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay,
                                    TaskType task_type) {
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return false;

  base::PendingTask pending_task(from_here, task, base::TimeTicks(),
                                 task_type != TaskType::NON_NESTABLE);
  task_queue_manager_->DidQueueTask(&pending_task);

  if (delay > base::TimeDelta()) {
    base::TimeTicks now = task_queue_manager_->Now();
    pending_task.delayed_run_time = now + delay;
    delayed_task_queue_.push(pending_task);
    TraceQueueSize(true);
    // If we changed the topmost task, then it is time to reschedule.
    if (delayed_task_queue_.top().task.Equals(pending_task.task)) {
      LazyNow lazy_now(now);
      ScheduleDelayedWorkLocked(&lazy_now);
    }
    return true;
  }
  EnqueueTaskLocked(pending_task);
  return true;
}

void TaskQueue::MoveReadyDelayedTasksToIncomingQueue() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);
  if (!task_queue_manager_)
    return;

  LazyNow lazy_now(task_queue_manager_);
  MoveReadyDelayedTasksToIncomingQueueLocked(&lazy_now);
}

void TaskQueue::MoveReadyDelayedTasksToIncomingQueueLocked(LazyNow* lazy_now) {
  lock_.AssertAcquired();
  // Enqueue all delayed tasks that should be running now.
  while (!delayed_task_queue_.empty() &&
         delayed_task_queue_.top().delayed_run_time <= lazy_now->Now()) {
    in_flight_kick_delayed_tasks_.erase(
        delayed_task_queue_.top().delayed_run_time);
    EnqueueTaskLocked(delayed_task_queue_.top());
    delayed_task_queue_.pop();
  }
  TraceQueueSize(true);
  ScheduleDelayedWorkLocked(lazy_now);
}

void TaskQueue::ScheduleDelayedWorkLocked(LazyNow* lazy_now) {
  lock_.AssertAcquired();
  // Any remaining tasks are in the future, so queue a task to kick them.
  if (!delayed_task_queue_.empty()) {
    base::TimeTicks next_run_time = delayed_task_queue_.top().delayed_run_time;
    DCHECK_GT(next_run_time, lazy_now->Now());
    // Make sure we don't have more than one
    // MoveReadyDelayedTasksToIncomingQueue posted for a particular scheduled
    // run time (note it's fine to have multiple ones in flight for distinct
    // run times).
    if (in_flight_kick_delayed_tasks_.find(next_run_time) ==
        in_flight_kick_delayed_tasks_.end()) {
      in_flight_kick_delayed_tasks_.insert(next_run_time);
      base::TimeDelta delay = next_run_time - lazy_now->Now();
      task_queue_manager_->PostDelayedTask(
          FROM_HERE,
          Bind(&TaskQueue::MoveReadyDelayedTasksToIncomingQueue, this), delay);
    }
  }
}

bool TaskQueue::IsQueueEmpty() const {
  if (!work_queue_.empty())
    return false;

  {
    base::AutoLock lock(lock_);
    return incoming_queue_.empty();
  }
}

bool TaskQueue::TaskIsOlderThanQueuedTasks(const base::PendingTask* task) {
  lock_.AssertAcquired();
  // A null task is passed when UpdateQueue is called before any task is run.
  // In this case we don't want to pump an after_wakeup queue, so return true
  // here.
  if (!task)
    return true;

  // Return false if there are no task in the incoming queue.
  if (incoming_queue_.empty())
    return false;

  base::PendingTask oldest_queued_task = incoming_queue_.front();
  DCHECK(oldest_queued_task.delayed_run_time.is_null());
  DCHECK(task->delayed_run_time.is_null());

  // Note: the comparison is correct due to the fact that the PendingTask
  // operator inverts its comparison operation in order to work well in a heap
  // based priority queue.
  return oldest_queued_task < *task;
}

bool TaskQueue::ShouldAutoPumpQueueLocked(
    const base::PendingTask* previous_task) {
  lock_.AssertAcquired();
  if (pump_policy_ == TaskQueueManager::PumpPolicy::MANUAL)
    return false;
  if (pump_policy_ == TaskQueueManager::PumpPolicy::AFTER_WAKEUP &&
      TaskIsOlderThanQueuedTasks(previous_task))
    return false;
  if (incoming_queue_.empty())
    return false;
  return true;
}

bool TaskQueue::NextPendingDelayedTaskRunTime(
    base::TimeTicks* next_pending_delayed_task) {
  base::AutoLock lock(lock_);
  if (delayed_task_queue_.empty())
    return false;
  *next_pending_delayed_task = delayed_task_queue_.top().delayed_run_time;
  return true;
}

bool TaskQueue::UpdateWorkQueue(LazyNow* lazy_now,
                                const base::PendingTask* previous_task) {
  if (!work_queue_.empty())
    return true;

  {
    base::AutoLock lock(lock_);
    MoveReadyDelayedTasksToIncomingQueueLocked(lazy_now);
    if (!ShouldAutoPumpQueueLocked(previous_task))
      return false;
    MoveReadyDelayedTasksToIncomingQueueLocked(lazy_now);
    work_queue_.Swap(&incoming_queue_);
    TraceQueueSize(true);
    return true;
  }
}

base::PendingTask TaskQueue::TakeTaskFromWorkQueue() {
  base::PendingTask pending_task = work_queue_.front();
  work_queue_.pop();
  TraceQueueSize(false);
  return pending_task;
}

void TaskQueue::TraceQueueSize(bool is_locked) const {
  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), &is_tracing);
  if (!is_tracing || !name_)
    return;
  if (!is_locked)
    lock_.Acquire();
  else
    lock_.AssertAcquired();
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), name_,
      incoming_queue_.size() + work_queue_.size() + delayed_task_queue_.size());
  if (!is_locked)
    lock_.Release();
}

void TaskQueue::EnqueueTaskLocked(const base::PendingTask& pending_task) {
  lock_.AssertAcquired();
  if (!task_queue_manager_)
    return;
  if (pump_policy_ == TaskQueueManager::PumpPolicy::AUTO &&
      incoming_queue_.empty())
    task_queue_manager_->MaybePostDoWorkOnMainRunner();
  incoming_queue_.push(pending_task);

  if (!pending_task.delayed_run_time.is_null()) {
    // Clear the delayed run time because we've already applied the delay
    // before getting here.
    incoming_queue_.back().delayed_run_time = base::TimeTicks();
  }
  TraceQueueSize(true);
}

void TaskQueue::SetPumpPolicy(TaskQueueManager::PumpPolicy pump_policy) {
  base::AutoLock lock(lock_);
  if (pump_policy == TaskQueueManager::PumpPolicy::AUTO &&
      pump_policy_ != TaskQueueManager::PumpPolicy::AUTO) {
    PumpQueueLocked();
  }
  pump_policy_ = pump_policy;
}

void TaskQueue::PumpQueueLocked() {
  lock_.AssertAcquired();
  if (task_queue_manager_) {
    LazyNow lazy_now(task_queue_manager_);
    MoveReadyDelayedTasksToIncomingQueueLocked(&lazy_now);
  }
  while (!incoming_queue_.empty()) {
    work_queue_.push(incoming_queue_.front());
    incoming_queue_.pop();
  }
  if (!work_queue_.empty())
    task_queue_manager_->MaybePostDoWorkOnMainRunner();
}

void TaskQueue::PumpQueue() {
  base::AutoLock lock(lock_);
  PumpQueueLocked();
}

void TaskQueue::AsValueInto(base::trace_event::TracedValue* state) const {
  base::AutoLock lock(lock_);
  state->BeginDictionary();
  if (name_)
    state->SetString("name", name_);
  state->SetString("pump_policy", PumpPolicyToString(pump_policy_));
  state->BeginArray("incoming_queue");
  QueueAsValueInto(incoming_queue_, state);
  state->EndArray();
  state->BeginArray("work_queue");
  QueueAsValueInto(work_queue_, state);
  state->BeginArray("delayed_task_queue");
  QueueAsValueInto(delayed_task_queue_, state);
  state->EndArray();
  state->EndDictionary();
}

// static
const char* TaskQueue::PumpPolicyToString(
    TaskQueueManager::PumpPolicy pump_policy) {
  switch (pump_policy) {
    case TaskQueueManager::PumpPolicy::AUTO:
      return "auto";
    case TaskQueueManager::PumpPolicy::AFTER_WAKEUP:
      return "after_wakeup";
    case TaskQueueManager::PumpPolicy::MANUAL:
      return "manual";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
void TaskQueue::QueueAsValueInto(const base::TaskQueue& queue,
                                 base::trace_event::TracedValue* state) {
  base::TaskQueue queue_copy(queue);
  while (!queue_copy.empty()) {
    TaskAsValueInto(queue_copy.front(), state);
    queue_copy.pop();
  }
}

// static
void TaskQueue::QueueAsValueInto(const base::DelayedTaskQueue& queue,
                                 base::trace_event::TracedValue* state) {
  base::DelayedTaskQueue queue_copy(queue);
  while (!queue_copy.empty()) {
    TaskAsValueInto(queue_copy.top(), state);
    queue_copy.pop();
  }
}

// static
void TaskQueue::TaskAsValueInto(const base::PendingTask& task,
                                base::trace_event::TracedValue* state) {
  state->BeginDictionary();
  state->SetString("posted_from", task.posted_from.ToString());
  state->SetInteger("sequence_num", task.sequence_num);
  state->SetBoolean("nestable", task.nestable);
  state->SetBoolean("is_high_res", task.is_high_res);
  state->SetDouble(
      "delayed_run_time",
      (task.delayed_run_time - base::TimeTicks()).InMicroseconds() / 1000.0L);
  state->EndDictionary();
}

}  // namespace internal

TaskQueueManager::TaskQueueManager(
    size_t task_queue_count,
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
    TaskQueueSelector* selector)
    : main_task_runner_(main_task_runner),
      selector_(selector),
      pending_dowork_count_(0),
      work_batch_size_(1),
      time_source_(nullptr),
      weak_factory_(this) {
  DCHECK(main_task_runner->RunsTasksOnCurrentThread());
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "TaskQueueManager",
      this);

  task_queue_manager_weak_ptr_ = weak_factory_.GetWeakPtr();
  for (size_t i = 0; i < task_queue_count; i++) {
    scoped_refptr<internal::TaskQueue> queue(
        make_scoped_refptr(new internal::TaskQueue(this)));
    queues_.push_back(queue);
  }

  std::vector<const base::TaskQueue*> work_queues;
  for (const auto& queue: queues_)
    work_queues.push_back(&queue->work_queue());
  selector_->RegisterWorkQueues(work_queues);
}

TaskQueueManager::~TaskQueueManager() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "TaskQueueManager",
      this);
  for (auto& queue : queues_)
    queue->WillDeleteTaskQueueManager();
}

internal::TaskQueue* TaskQueueManager::Queue(size_t queue_index) const {
  DCHECK_LT(queue_index, queues_.size());
  return queues_[queue_index].get();
}

scoped_refptr<base::SingleThreadTaskRunner>
TaskQueueManager::TaskRunnerForQueue(size_t queue_index) const {
  return Queue(queue_index);
}

bool TaskQueueManager::IsQueueEmpty(size_t queue_index) const {
  internal::TaskQueue* queue = Queue(queue_index);
  return queue->IsQueueEmpty();
}

base::TimeTicks TaskQueueManager::NextPendingDelayedTaskRunTime() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  bool found_pending_task = false;
  base::TimeTicks next_pending_delayed_task(
      base::TimeTicks::FromInternalValue(kMaxTimeTicks));
  for (auto& queue : queues_) {
    base::TimeTicks queues_next_pending_delayed_task;
    if (queue->NextPendingDelayedTaskRunTime(
            &queues_next_pending_delayed_task)) {
      found_pending_task = true;
      next_pending_delayed_task =
          std::min(next_pending_delayed_task, queues_next_pending_delayed_task);
    }
  }

  if (!found_pending_task)
    return base::TimeTicks();

  DCHECK_NE(next_pending_delayed_task,
            base::TimeTicks::FromInternalValue(kMaxTimeTicks));
  return next_pending_delayed_task;
}

void TaskQueueManager::SetPumpPolicy(size_t queue_index,
                                     PumpPolicy pump_policy) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  internal::TaskQueue* queue = Queue(queue_index);
  queue->SetPumpPolicy(pump_policy);
}

void TaskQueueManager::PumpQueue(size_t queue_index) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  internal::TaskQueue* queue = Queue(queue_index);
  queue->PumpQueue();
}

bool TaskQueueManager::UpdateWorkQueues(
    const base::PendingTask* previous_task) {
  // TODO(skyostil): This is not efficient when the number of queues grows very
  // large due to the number of locks taken. Consider optimizing when we get
  // there.
  DCHECK(main_thread_checker_.CalledOnValidThread());
  internal::LazyNow lazy_now(this);
  bool has_work = false;
  for (auto& queue : queues_) {
    has_work |= queue->UpdateWorkQueue(&lazy_now, previous_task);
    if (!queue->work_queue().empty()) {
      // Currently we should not be getting tasks with delayed run times in any
      // of the work queues.
      DCHECK(queue->work_queue().front().delayed_run_time.is_null());
    }
  }
  return has_work;
}

void TaskQueueManager::MaybePostDoWorkOnMainRunner() {
  bool on_main_thread = main_task_runner_->BelongsToCurrentThread();
  if (on_main_thread) {
    // We only want one pending DoWork posted from the main thread, or we risk
    // an explosion of pending DoWorks which could starve out everything else.
    if (pending_dowork_count_ > 0) {
      return;
    }
    pending_dowork_count_++;
  }

  main_task_runner_->PostTask(
      FROM_HERE, Bind(&TaskQueueManager::DoWork, task_queue_manager_weak_ptr_,
                      on_main_thread));
}

void TaskQueueManager::DoWork(bool posted_from_main_thread) {
  if (posted_from_main_thread) {
    pending_dowork_count_--;
    DCHECK_GE(pending_dowork_count_, 0);
  }
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Pass nullptr to UpdateWorkQueues here to prevent waking up a
  // pump-after-wakeup queue.
  if (!UpdateWorkQueues(nullptr))
    return;

  base::PendingTask previous_task((tracked_objects::Location()),
                                  (base::Closure()));
  for (int i = 0; i < work_batch_size_; i++) {
    size_t queue_index;
    if (!SelectWorkQueueToService(&queue_index))
      return;
    // Note that this function won't post another call to DoWork if one is
    // already pending, so it is safe to call it in a loop.
    MaybePostDoWorkOnMainRunner();
    ProcessTaskFromWorkQueue(queue_index, i > 0, &previous_task);

    if (!UpdateWorkQueues(&previous_task))
      return;
  }
}

bool TaskQueueManager::SelectWorkQueueToService(size_t* out_queue_index) {
  bool should_run = selector_->SelectWorkQueueToService(out_queue_index);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "TaskQueueManager", this,
      AsValueWithSelectorResult(should_run, *out_queue_index));
  return should_run;
}

void TaskQueueManager::DidQueueTask(base::PendingTask* pending_task) {
  pending_task->sequence_num = task_sequence_num_.GetNext();
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", *pending_task);
}

void TaskQueueManager::ProcessTaskFromWorkQueue(
    size_t queue_index,
    bool has_previous_task,
    base::PendingTask* previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  internal::TaskQueue* queue = Queue(queue_index);
  base::PendingTask pending_task = queue->TakeTaskFromWorkQueue();
  if (!pending_task.nestable && main_task_runner_->IsNested()) {
    // Defer non-nestable work to the main task runner.  NOTE these tasks can be
    // arbitrarily delayed so the additional delay should not be a problem.
    main_task_runner_->PostNonNestableTask(pending_task.posted_from,
                                           pending_task.task);
  } else {
    // Suppress "will" task observer notifications for the first and "did"
    // notifications for the last task in the batch to avoid duplicate
    // notifications.
    if (has_previous_task) {
      FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                        DidProcessTask(*previous_task));
      FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                        WillProcessTask(pending_task));
    }
    task_annotator_.RunTask("TaskQueueManager::PostTask",
                            "TaskQueueManager::RunTask", pending_task);
    pending_task.task.Reset();
    *previous_task = pending_task;
  }
}

bool TaskQueueManager::RunsTasksOnCurrentThread() const {
  return main_task_runner_->RunsTasksOnCurrentThread();
}

bool TaskQueueManager::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  DCHECK(delay > base::TimeDelta());
  return main_task_runner_->PostDelayedTask(from_here, task, delay);
}

void TaskQueueManager::SetQueueName(size_t queue_index, const char* name) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  internal::TaskQueue* queue = Queue(queue_index);
  queue->set_name(name);
}

void TaskQueueManager::SetWorkBatchSize(int work_batch_size) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_GE(work_batch_size, 1);
  work_batch_size_ = work_batch_size;
}

void TaskQueueManager::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->AddTaskObserver(task_observer);
  task_observers_.AddObserver(task_observer);
}

void TaskQueueManager::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::MessageLoop::current()->RemoveTaskObserver(task_observer);
  task_observers_.RemoveObserver(task_observer);
}

void TaskQueueManager::SetTimeSourceForTesting(
    scoped_refptr<cc::TestNowSource> time_source) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  time_source_ = time_source;
}

base::TimeTicks TaskQueueManager::Now() const {
  return UNLIKELY(time_source_) ? time_source_->Now() : base::TimeTicks::Now();
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
TaskQueueManager::AsValueWithSelectorResult(bool should_run,
                                            size_t selected_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  state->BeginArray("queues");
  for (auto& queue : queues_)
    queue->AsValueInto(state.get());
  state->EndArray();
  state->BeginDictionary("selector");
  selector_->AsValueInto(state.get());
  state->EndDictionary();
  if (should_run)
    state->SetInteger("selected_queue", selected_queue);
  return state;
}

}  // namespace content
