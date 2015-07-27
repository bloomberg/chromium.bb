// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/task_queue_manager.h"

#include <queue>
#include <set>

#include "base/bind.h"
#include "base/time/default_tick_clock.h"
#include "components/scheduler/child/lazy_now.h"
#include "components/scheduler/child/nestable_single_thread_task_runner.h"
#include "components/scheduler/child/task_queue_impl.h"
#include "components/scheduler/child/task_queue_selector.h"
#include "components/scheduler/child/task_queue_sets.h"

namespace {
const int64_t kMaxTimeTicks = std::numeric_limits<int64>::max();
}

namespace scheduler {

TaskQueueManager::TaskQueueManager(
    scoped_refptr<NestableSingleThreadTaskRunner> main_task_runner,
    const char* disabled_by_default_tracing_category,
    const char* disabled_by_default_verbose_tracing_category)
    : main_task_runner_(main_task_runner),
      task_was_run_on_quiescence_monitored_queue_(false),
      pending_dowork_count_(0),
      work_batch_size_(1),
      time_source_(new base::DefaultTickClock),
      disabled_by_default_tracing_category_(
          disabled_by_default_tracing_category),
      disabled_by_default_verbose_tracing_category_(
          disabled_by_default_verbose_tracing_category),
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
}

TaskQueueManager::~TaskQueueManager() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(disabled_by_default_tracing_category_,
                                     "TaskQueueManager", this);
  for (auto& queue : queues_)
    queue->WillDeleteTaskQueueManager();
  selector_.SetTaskQueueSelectorObserver(nullptr);
}

scoped_refptr<internal::TaskQueueImpl> TaskQueueManager::NewTaskQueue(
    const TaskQueue::Spec& spec) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<internal::TaskQueueImpl> queue(
      make_scoped_refptr(new internal::TaskQueueImpl(
          this, spec, disabled_by_default_tracing_category_,
          disabled_by_default_verbose_tracing_category_)));
  queues_.insert(queue.get());
  selector_.AddQueue(queue.get());
  return queue;
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
  updatable_queue_set_.erase(queue);
}

void TaskQueueManager::UpdateWorkQueues(
    bool should_trigger_wakeup,
    const base::PendingTask* previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  internal::LazyNow lazy_now(this);

  // Insert any newly updatable queues into the updatable_queue_set_.
  {
    base::AutoLock lock(newly_updatable_lock_);
    while (!newly_updatable_.empty()) {
      updatable_queue_set_.insert(newly_updatable_.back());
      newly_updatable_.pop_back();
    }
  }

  auto iter = updatable_queue_set_.begin();
  while (iter != updatable_queue_set_.end()) {
    internal::TaskQueueImpl* queue = *iter++;
    // NOTE Update work queue may erase itself from |updatable_queue_set_|.
    // This is fine, erasing an element won't invalidate any interator, as long
    // as the iterator isn't the element being delated.
    if (queue->work_queue().empty())
        queue->UpdateWorkQueue(&lazy_now, should_trigger_wakeup, previous_task);
    if (!queue->work_queue().empty()) {
      // Currently we should not be getting tasks with delayed run times in any
      // of the work queues.
      DCHECK(queue->work_queue().front().delayed_run_time.is_null());
    }
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

void TaskQueueManager::DoWork(bool posted_from_main_thread) {
  if (posted_from_main_thread) {
    pending_dowork_count_--;
    DCHECK_GE(pending_dowork_count_, 0);
  }
  DCHECK(main_thread_checker_.CalledOnValidThread());

  // Pass false and nullptr to UpdateWorkQueues here to prevent waking up a
  // pump-after-wakeup queue.
  UpdateWorkQueues(false, nullptr);

  base::PendingTask previous_task((tracked_objects::Location()),
                                  (base::Closure()));
  for (int i = 0; i < work_batch_size_; i++) {
    internal::TaskQueueImpl* queue;
    if (!SelectQueueToService(&queue))
      return;
    // Note that this function won't post another call to DoWork if one is
    // already pending, so it is safe to call it in a loop.
    MaybePostDoWorkOnMainRunner();

    if (ProcessTaskFromWorkQueue(queue, &previous_task))
      return;  // The TaskQueueManager got deleted, we must bail out.

    bool should_trigger_wakeup = queue->wakeup_policy() ==
                                 TaskQueue::WakeupPolicy::CAN_WAKE_OTHER_QUEUES;
    UpdateWorkQueues(should_trigger_wakeup, &previous_task);

    // Only run a single task per batch in nested run loops so that we can
    // properly exit the nested loop when someone calls RunLoop::Quit().
    if (main_task_runner_->IsNested())
      break;
  }
}

bool TaskQueueManager::SelectQueueToService(
    internal::TaskQueueImpl** out_queue) {
  bool should_run = selector_.SelectQueueToService(out_queue);
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      disabled_by_default_tracing_category_, "TaskQueueManager", this,
      AsValueWithSelectorResult(should_run, *out_queue));
  return should_run;
}

void TaskQueueManager::DidQueueTask(const base::PendingTask& pending_task) {
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", pending_task);
}

bool TaskQueueManager::ProcessTaskFromWorkQueue(
    internal::TaskQueueImpl* queue,
    base::PendingTask* out_previous_task) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  scoped_refptr<DeletionSentinel> protect(deletion_sentinel_);
  base::PendingTask pending_task = queue->TakeTaskFromWorkQueue();

  if (queue->GetQuiescenceMonitored())
    task_was_run_on_quiescence_monitored_queue_ = true;

  if (!pending_task.nestable && main_task_runner_->IsNested()) {
    // Defer non-nestable work to the main task runner.  NOTE these tasks can be
    // arbitrarily delayed so the additional delay should not be a problem.
    main_task_runner_->PostNonNestableTask(pending_task.posted_from,
                                           pending_task.task);
  } else {
    TRACE_TASK_EXECUTION("TaskQueueManager::ProcessTaskFromWorkQueue",
                         pending_task);
    if (queue->GetShouldNotifyObservers()) {
      FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                        WillProcessTask(pending_task));
    }
    task_annotator_.RunTask("TaskQueueManager::PostTask", pending_task);

    // Detect if the TaskQueueManager just got deleted.  If this happens we must
    // not access any member variables after this point.
    if (protect->HasOneRef())
      return true;

    if (queue->GetShouldNotifyObservers()) {
      FOR_EACH_OBSERVER(base::MessageLoop::TaskObserver, task_observers_,
                        DidProcessTask(pending_task));
    }

    pending_task.task.Reset();
    *out_previous_task = pending_task;
  }
  return false;
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

void TaskQueueManager::OnTaskQueueEnabled() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  MaybePostDoWorkOnMainRunner();
}

}  // namespace scheduler
