// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/base/task_queue_manager.h"

#include <queue>
#include <set>

#include "base/bind.h"
#include "base/time/default_tick_clock.h"
#include "components/scheduler/base/lazy_now.h"
#include "components/scheduler/base/nestable_single_thread_task_runner.h"
#include "components/scheduler/base/task_queue_impl.h"
#include "components/scheduler/base/task_queue_selector.h"
#include "components/scheduler/base/task_queue_sets.h"

namespace {
const int64_t kMaxTimeTicks = std::numeric_limits<int64>::max();
}

namespace scheduler {

TaskQueueManager::TaskQueueManager(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
    const char* tracing_category,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : main_task_runner_(main_task_runner),
      task_was_run_on_quiescence_monitored_queue_(false),
      pending_dowork_count_(0),
      work_batch_size_(1),
      time_source_(new base::DefaultTickClock),
      tracing_category_(tracing_category),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
      observer_(nullptr),
      deletion_sentinel_(new DeletionSentinel()),
      weak_factory_(this) {
  DCHECK(main_task_runner->RunsTasksOnCurrentThread());
  TRACE_EVENT_OBJECT_CREATED_WITH_ID(disabled_by_default_tracing_category,
                                     "TaskQueueManager", this);
  selector_.SetTaskQueueSelectorObserver(this);

  do_work_from_main_thread_closure_ =
      base::Bind(&TaskQueueManager::DoWork, weak_factory_.GetWeakPtr(), true);
  do_work_from_other_thread_closure_ =
      base::Bind(&TaskQueueManager::DoWork, weak_factory_.GetWeakPtr(), false);
  delayed_queue_wakeup_closure_ =
      base::Bind(&TaskQueueManager::DelayedDoWork, weak_factory_.GetWeakPtr());
}

TaskQueueManager::~TaskQueueManager() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(disabled_by_default_tracing_category_,
                                     "TaskQueueManager", this);

  while (!queues_.empty())
    (*queues_.begin())->UnregisterTaskQueue();

  selector_.SetTaskQueueSelectorObserver(nullptr);
}

scoped_refptr<internal::TaskQueueImpl> TaskQueueManager::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  TRACE_EVENT1(tracing_category_,
               "TaskQueueManager::NewTaskQueue", "queue_name", spec.name);
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<internal::TaskQueueImpl> queue(
      make_scoped_refptr(new internal::TaskQueueImpl(
          this, spec, disabled_by_default_tracing_category_,
          disabled_by_default_verbose_tracing_category_)));
  queues_.insert(queue);
  selector_.AddQueue(queue.get());
  return queue;
}

void TaskQueueManager::SetObserver(Observer* observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  observer_ = observer;
}

void TaskQueueManager::UnregisterTaskQueue(
    scoped_refptr<internal::TaskQueueImpl> task_queue) {
  TRACE_EVENT1(tracing_category_,
               "TaskQueueManager::UnregisterTaskQueue", "queue_name",
               task_queue->GetName());
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (observer_)
    observer_->OnUnregisterTaskQueue(task_queue);

  // Add |task_queue| to |queues_to_delete_| so we can prevent it from being
  // freed while any of our structures hold hold a raw pointer to it.
  queues_to_delete_.insert(task_queue);
  queues_.erase(task_queue);
  selector_.RemoveQueue(task_queue.get());

  // We need to remove |task_queue| from delayed_wakeup_multimap_ which is a
  // little awkward since it's keyed by time. O(n) running time.
  for (DelayedWakeupMultimap::iterator iter = delayed_wakeup_multimap_.begin();
       iter != delayed_wakeup_multimap_.end();) {
    if (iter->second == task_queue.get()) {
      DelayedWakeupMultimap::iterator temp = iter;
      iter++;
      // O(1) amortized.
      delayed_wakeup_multimap_.erase(temp);
    } else {
      iter++;
    }
  }

  // |newly_updatable_| might contain |task_queue|, we use
  // MoveNewlyUpdatableQueuesIntoUpdatableQueueSet to flush it out.
  MoveNewlyUpdatableQueuesIntoUpdatableQueueSet();
  updatable_queue_set_.erase(task_queue.get());
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

void TaskQueueManager::RegisterAsUpdatableTaskQueue(
    internal::TaskQueueImpl* queue) {
  base::AutoLock lock(newly_updatable_lock_);
  newly_updatable_.push_back(queue);
}

void TaskQueueManager::UnregisterAsUpdatableTaskQueue(
    internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  MoveNewlyUpdatableQueuesIntoUpdatableQueueSet();
#ifndef NDEBUG
  {
    base::AutoLock lock(newly_updatable_lock_);
    DCHECK(!(updatable_queue_set_.find(queue) == updatable_queue_set_.end() &&
             std::find(newly_updatable_.begin(), newly_updatable_.end(),
                       queue) != newly_updatable_.end()));
  }
#endif
  updatable_queue_set_.erase(queue);
}

void TaskQueueManager::MoveNewlyUpdatableQueuesIntoUpdatableQueueSet() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  base::AutoLock lock(newly_updatable_lock_);
  while (!newly_updatable_.empty()) {
    updatable_queue_set_.insert(newly_updatable_.back());
    newly_updatable_.pop_back();
  }
}

void TaskQueueManager::UpdateWorkQueues(
    bool should_trigger_wakeup,
    const internal::TaskQueueImpl::Task* previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  TRACE_EVENT0(disabled_by_default_tracing_category_,
               "TaskQueueManager::UpdateWorkQueues");
  internal::LazyNow lazy_now(this);

  // Move any ready delayed tasks into the incomming queues.
  WakeupReadyDelayedQueues(&lazy_now);

  MoveNewlyUpdatableQueuesIntoUpdatableQueueSet();

  auto iter = updatable_queue_set_.begin();
  while (iter != updatable_queue_set_.end()) {
    internal::TaskQueueImpl* queue = *iter++;
    // NOTE Update work queue may erase itself from |updatable_queue_set_|.
    // This is fine, erasing an element won't invalidate any interator, as long
    // as the iterator isn't the element being delated.
    if (queue->work_queue().empty())
      queue->UpdateWorkQueue(&lazy_now, should_trigger_wakeup, previous_task);
  }
}

void TaskQueueManager::ScheduleDelayedWorkTask(
    scoped_refptr<internal::TaskQueueImpl> queue,
    base::TimeTicks delayed_run_time) {
  internal::LazyNow lazy_now(this);
  ScheduleDelayedWork(queue.get(), delayed_run_time, &lazy_now);
}

void TaskQueueManager::ScheduleDelayedWork(internal::TaskQueueImpl* queue,
                                           base::TimeTicks delayed_run_time,
                                           internal::LazyNow* lazy_now) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    // NOTE posting a delayed task from a different thread is not expected to be
    // common. This pathway is less optimal than perhaps it could be because
    // it causes two main thread tasks to be run.  Should this assumption prove
    // to be false in future, we may need to revisit this.
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&TaskQueueManager::ScheduleDelayedWorkTask,
                              weak_factory_.GetWeakPtr(),
                              scoped_refptr<internal::TaskQueueImpl>(queue),
                              delayed_run_time));
    return;
  }

  // Make sure there's one (and only one) task posted to |main_task_runner_|
  // to call |DelayedDoWork| at |delayed_run_time|.
  if (delayed_wakeup_multimap_.find(delayed_run_time) ==
      delayed_wakeup_multimap_.end()) {
    base::TimeDelta delay =
        std::max(base::TimeDelta(), delayed_run_time - lazy_now->Now());
    main_task_runner_->PostDelayedTask(FROM_HERE, delayed_queue_wakeup_closure_,
                                       delay);
  }
  delayed_wakeup_multimap_.insert(std::make_pair(delayed_run_time, queue));
}

void TaskQueueManager::DelayedDoWork() {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  {
    internal::LazyNow lazy_now(this);
    WakeupReadyDelayedQueues(&lazy_now);
  }

  DoWork(false);
}

void TaskQueueManager::WakeupReadyDelayedQueues(internal::LazyNow* lazy_now) {
  // Wake up any queues with pending delayed work.  Note std::multipmap stores
  // the elements sorted by key, so the begin() iterator points to the earliest
  // queue to wakeup.
  std::set<internal::TaskQueueImpl*> dedup_set;
  while (!delayed_wakeup_multimap_.empty()) {
    DelayedWakeupMultimap::iterator next_wakeup =
        delayed_wakeup_multimap_.begin();
    if (next_wakeup->first > lazy_now->Now())
      break;
    // A queue could have any number of delayed tasks pending so it's worthwhile
    // deduping calls to MoveReadyDelayedTasksToIncomingQueue since it takes a
    // lock.  NOTE the order in which these are called matters since the order
    // in which EnqueueTaskLocks is called is respected when choosing which
    // queue to execute a task from.
    if (dedup_set.insert(next_wakeup->second).second)
      next_wakeup->second->MoveReadyDelayedTasksToIncomingQueue(lazy_now);
    delayed_wakeup_multimap_.erase(next_wakeup);
  }
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
    main_task_runner_->PostTask(FROM_HERE, do_work_from_main_thread_closure_);
  } else {
    main_task_runner_->PostTask(FROM_HERE, do_work_from_other_thread_closure_);
  }
}

void TaskQueueManager::DoWork(bool decrement_pending_dowork_count) {
  if (decrement_pending_dowork_count) {
    pending_dowork_count_--;
    DCHECK_GE(pending_dowork_count_, 0);
  }
  DCHECK(main_thread_checker_.CalledOnValidThread());

  if (!main_task_runner_->IsNested())
    queues_to_delete_.clear();

  // Pass false and nullptr to UpdateWorkQueues here to prevent waking up a
  // pump-after-wakeup queue.
  UpdateWorkQueues(false, nullptr);

  internal::TaskQueueImpl::Task previous_task;
  for (int i = 0; i < work_batch_size_; i++) {
    internal::TaskQueueImpl* queue;
    if (!SelectQueueToService(&queue))
      break;

    switch (ProcessTaskFromWorkQueue(queue, &previous_task)) {
      case ProcessTaskResult::DEFERRED:
        // If a task was deferred, try again with another task. Note that this
        // means deferred tasks (i.e. non-nestable tasks) will never trigger
        // queue wake-ups.
        continue;
      case ProcessTaskResult::EXECUTED:
        break;
      case ProcessTaskResult::TASK_QUEUE_MANAGER_DELETED:
        return;  // The TaskQueueManager got deleted, we must bail out.
    }
    bool should_trigger_wakeup = queue->wakeup_policy() ==
                                 TaskQueue::WakeupPolicy::CAN_WAKE_OTHER_QUEUES;
    UpdateWorkQueues(should_trigger_wakeup, &previous_task);

    // Only run a single task per batch in nested run loops so that we can
    // properly exit the nested loop when someone calls RunLoop::Quit().
    if (main_task_runner_->IsNested())
      break;
  }

  // TODO(alexclarke): Consider refactoring the above loop to terminate only
  // when there's no more work left to be done, rather than posting a
  // continuation task.
  if (!selector_.EnabledWorkQueuesEmpty())
    MaybePostDoWorkOnMainRunner();
}

bool TaskQueueManager::SelectQueueToService(
    internal::TaskQueueImpl** out_queue) {
  bool should_run = selector_.SelectQueueToService(out_queue);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      disabled_by_default_tracing_category_, "TaskQueueManager", this,
      AsValueWithSelectorResult(should_run, *out_queue));
  return should_run;
}

void TaskQueueManager::DidQueueTask(
    const internal::TaskQueueImpl::Task& pending_task) {
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", pending_task);
}

TaskQueueManager::ProcessTaskResult TaskQueueManager::ProcessTaskFromWorkQueue(
    internal::TaskQueueImpl* queue,
    internal::TaskQueueImpl::Task* out_previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<DeletionSentinel> protect(deletion_sentinel_);
  // TODO(alexclarke): consider std::move() when allowed.
  internal::TaskQueueImpl::Task pending_task = queue->TakeTaskFromWorkQueue();

  if (queue->GetQuiescenceMonitored())
    task_was_run_on_quiescence_monitored_queue_ = true;

  if (!pending_task.nestable && main_task_runner_->IsNested()) {
    // Defer non-nestable work to the main task runner.  NOTE these tasks can be
    // arbitrarily delayed so the additional delay should not be a problem.
    // TODO(skyostil): Figure out a way to not forget which task queue the
    // task is associated with. See http://crbug.com/522843.
    main_task_runner_->PostNonNestableTask(pending_task.posted_from,
                                           pending_task.task);
    return ProcessTaskResult::DEFERRED;
  }

  TRACE_TASK_EXECUTION("TaskQueueManager::ProcessTaskFromWorkQueue",
                       pending_task);
  if (queue->GetShouldNotifyObservers()) {
    FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                      WillProcessTask(pending_task));
    queue->NotifyWillProcessTask(pending_task);
  }
  TRACE_EVENT1(tracing_category_,
               "TaskQueueManager::RunTask", "queue", queue->GetName());
  task_annotator_.RunTask("TaskQueueManager::PostTask", pending_task);

  // Detect if the TaskQueueManager just got deleted.  If this happens we must
  // not access any member variables after this point.
  if (protect->HasOneRef())
    return ProcessTaskResult::TASK_QUEUE_MANAGER_DELETED;

  if (queue->GetShouldNotifyObservers()) {
    FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                      DidProcessTask(pending_task));
    queue->NotifyDidProcessTask(pending_task);
  }

  pending_task.task.Reset();
  *out_previous_task = pending_task;
  return ProcessTaskResult::EXECUTED;
}

bool TaskQueueManager::RunsTasksOnCurrentThread() const {
  return main_task_runner_->RunsTasksOnCurrentThread();
}

bool TaskQueueManager::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta());
  return main_task_runner_->PostDelayedTask(from_here, task, delay);
}

void TaskQueueManager::SetWorkBatchSize(int work_batch_size) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK_GE(work_batch_size, 1);
  work_batch_size_ = work_batch_size;
}

void TaskQueueManager::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_observers_.AddObserver(task_observer);
}

void TaskQueueManager::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  task_observers_.RemoveObserver(task_observer);
}

void TaskQueueManager::SetTimeSourceForTesting(
    scoped_ptr<base::TickClock> time_source) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  time_source_ = time_source.Pass();
}

bool TaskQueueManager::GetAndClearSystemIsQuiescentBit() {
  bool task_was_run = task_was_run_on_quiescence_monitored_queue_;
  task_was_run_on_quiescence_monitored_queue_ = false;
  return !task_was_run;
}

base::TimeTicks TaskQueueManager::Now() const {
  return time_source_->NowTicks();
}

int TaskQueueManager::GetNextSequenceNumber() {
  return task_sequence_num_.GetNext();
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
TaskQueueManager::AsValueWithSelectorResult(
    bool should_run,
    internal::TaskQueueImpl* selected_queue) const {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  state->BeginArray("queues");
  for (auto& queue : queues_)
    queue->AsValueInto(state.get());
  state->EndArray();
  state->BeginDictionary("selector");
  selector_.AsValueInto(state.get());
  state->EndDictionary();
  if (should_run)
    state->SetString("selected_queue", selected_queue->GetName());

  state->BeginArray("updatable_queue_set");
  for (auto& queue : updatable_queue_set_)
    state->AppendString(queue->GetName());
  state->EndArray();
  return state;
}

void TaskQueueManager::OnTaskQueueEnabled(internal::TaskQueueImpl* queue) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  // Only schedule DoWork if there's something to do.
  if (!queue->work_queue().empty())
    MaybePostDoWorkOnMainRunner();
}

}  // namespace scheduler
