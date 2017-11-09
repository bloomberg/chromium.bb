// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler.h"

#include <algorithm>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "gpu/command_buffer/service/sync_point_manager.h"

namespace gpu {

class Scheduler::Sequence {
 public:
  Sequence(SequenceId sequence_id,
           SchedulingPriority priority,
           scoped_refptr<SyncPointOrderData> order_data);

  ~Sequence();

  SequenceId sequence_id() const { return sequence_id_; }

  const scoped_refptr<SyncPointOrderData>& order_data() const {
    return order_data_;
  }

  bool enabled() const { return enabled_; }

  bool scheduled() const { return running_state_ == SCHEDULED; }

  bool running() const { return running_state_ == RUNNING; }

  // The sequence is runnable if its enabled and has tasks which are not blocked
  // by wait fences.
  bool IsRunnable() const;

  // Returns true if this sequence's scheduling state changed and it needs to be
  // reinserted into the scheduling queue.
  bool NeedsRescheduling() const;

  // Returns true if this sequence should yield to another sequence. Uses the
  // cached scheduling state for comparison.
  bool ShouldYieldTo(const Sequence* other) const;

  // Enables or disables the sequence.
  void SetEnabled(bool enabled);

  // Sets running state to SCHEDULED. Returns scheduling state for this sequence
  // used for inserting in the scheduling queue.
  SchedulingState SetScheduled();

  // Update cached scheduling priority while running.
  void UpdateRunningPriority();

  // Returns the next order number and closure. Sets running state to RUNNING.
  uint32_t BeginTask(base::OnceClosure* closure);

  // Called after running the closure returned by BeginTask. Sets running state
  // to IDLE.
  void FinishTask();

  // Enqueues a task in the sequence and returns the generated order number.
  uint32_t ScheduleTask(base::OnceClosure closure);

  // Continue running the current task with the given closure. Must be called in
  // between |BeginTask| and |FinishTask|.
  void ContinueTask(base::OnceClosure closure);

  // Add a sync token fence that this sequence should wait on.
  void AddWaitFence(const SyncToken& sync_token, uint32_t order_num);

  // Remove a waiting sync token fence.
  void RemoveWaitFence(const SyncToken& sync_token, uint32_t order_num);

  // Add a sync token fence that this sequence is expected to release.
  void AddReleaseFence(const SyncToken& sync_token, uint32_t order_num);

  // Remove a release sync token fence.
  void RemoveReleaseFence(const SyncToken& sync_token, uint32_t order_num);

  void AddClientWait(CommandBufferId command_buffer_id);

  void RemoveClientWait(CommandBufferId command_buffer_id);

 private:
  enum RunningState { IDLE, SCHEDULED, RUNNING };

  struct Fence {
    SyncToken sync_token;
    uint32_t order_num;

    bool operator==(const Fence& other) const {
      return std::tie(sync_token, order_num) ==
             std::tie(other.sync_token, other.order_num);
    }
  };

  struct Task {
    base::OnceClosure closure;
    uint32_t order_num;
  };

  SchedulingPriority GetSchedulingPriority() const;

  // If the sequence is enabled. Sequences are disabled/enabled based on when
  // the command buffer is descheduled/scheduled.
  bool enabled_ = true;

  RunningState running_state_ = IDLE;

  // Cached scheduling state used for comparison with other sequences while
  // running. Updated in |SetScheduled| and |UpdateRunningPriority|.
  SchedulingState scheduling_state_;

  const SequenceId sequence_id_;

  const SchedulingPriority priority_;

  scoped_refptr<SyncPointOrderData> order_data_;

  // Deque of tasks. Tasks are inserted at the back with increasing order number
  // generated from SyncPointOrderData. If a running task needs to be continued,
  // it is inserted at the front with the same order number.
  base::circular_deque<Task> tasks_;

  // List of fences that this sequence is waiting on. Fences are inserted in
  // increasing order number but may be removed out of order. Tasks are blocked
  // if there's a wait fence with order number less than or equal to the task's
  // order number.
  std::vector<Fence> wait_fences_;

  // List of fences that this sequence is expected to release. If this list is
  // non-empty, the priority of the sequence is raised.
  std::vector<Fence> release_fences_;

  base::flat_set<CommandBufferId> client_waits_;

  DISALLOW_COPY_AND_ASSIGN(Sequence);
};

Scheduler::Task::Task(SequenceId sequence_id,
                      base::OnceClosure closure,
                      std::vector<SyncToken> sync_token_fences)
    : sequence_id(sequence_id),
      closure(std::move(closure)),
      sync_token_fences(std::move(sync_token_fences)) {}
Scheduler::Task::Task(Task&& other) = default;
Scheduler::Task::~Task() = default;
Scheduler::Task& Scheduler::Task::operator=(Task&& other) = default;

Scheduler::SchedulingState::SchedulingState() = default;
Scheduler::SchedulingState::SchedulingState(const SchedulingState& other) =
    default;
Scheduler::SchedulingState::~SchedulingState() = default;

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
Scheduler::SchedulingState::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetInteger("sequence_id", sequence_id.GetUnsafeValue());
  state->SetString("priority", SchedulingPriorityToString(priority));
  state->SetInteger("order_num", order_num);
  return std::move(state);
}

Scheduler::Sequence::Sequence(SequenceId sequence_id,
                              SchedulingPriority priority,
                              scoped_refptr<SyncPointOrderData> order_data)
    : sequence_id_(sequence_id),
      priority_(priority),
      order_data_(std::move(order_data)) {}

Scheduler::Sequence::~Sequence() {
  order_data_->Destroy();
}

SchedulingPriority Scheduler::Sequence::GetSchedulingPriority() const {
  SchedulingPriority priority = priority_;
  if (!release_fences_.empty() || !client_waits_.empty())
    priority = std::min(priority, SchedulingPriority::kHigh);
  return priority;
}

bool Scheduler::Sequence::NeedsRescheduling() const {
  return running_state_ != IDLE &&
         scheduling_state_.priority != GetSchedulingPriority();
}

bool Scheduler::Sequence::IsRunnable() const {
  return enabled_ && !tasks_.empty() &&
         (wait_fences_.empty() ||
          wait_fences_.front().order_num > tasks_.front().order_num);
}

bool Scheduler::Sequence::ShouldYieldTo(const Sequence* other) const {
  if (!running() || !other->scheduled())
    return false;
  return other->scheduling_state_.RunsBefore(scheduling_state_);
}

void Scheduler::Sequence::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  DCHECK_EQ(running_state_, enabled ? IDLE : RUNNING);
  enabled_ = enabled;
  if (enabled) {
    TRACE_EVENT_ASYNC_BEGIN1("gpu", "SequenceEnabled", this, "sequence_id",
                             sequence_id_.GetUnsafeValue());
  } else {
    TRACE_EVENT_ASYNC_END1("gpu", "SequenceEnabled", this, "sequence_id",
                           sequence_id_.GetUnsafeValue());
  }
}

Scheduler::SchedulingState Scheduler::Sequence::SetScheduled() {
  DCHECK(IsRunnable());
  DCHECK_NE(running_state_, RUNNING);

  running_state_ = SCHEDULED;

  scheduling_state_.sequence_id = sequence_id_;
  scheduling_state_.priority = GetSchedulingPriority();
  scheduling_state_.order_num = tasks_.front().order_num;

  return scheduling_state_;
}

void Scheduler::Sequence::UpdateRunningPriority() {
  DCHECK_EQ(running_state_, RUNNING);
  scheduling_state_.priority = GetSchedulingPriority();
}

void Scheduler::Sequence::ContinueTask(base::OnceClosure closure) {
  DCHECK_EQ(running_state_, RUNNING);
  uint32_t order_num = order_data_->current_order_num();
  tasks_.push_front({std::move(closure), order_num});
  order_data_->PauseProcessingOrderNumber(order_num);
}

uint32_t Scheduler::Sequence::ScheduleTask(base::OnceClosure closure) {
  uint32_t order_num = order_data_->GenerateUnprocessedOrderNumber();
  tasks_.push_back({std::move(closure), order_num});
  return order_num;
}

uint32_t Scheduler::Sequence::BeginTask(base::OnceClosure* closure) {
  DCHECK(closure);
  DCHECK(!tasks_.empty());
  DCHECK_EQ(running_state_, SCHEDULED);

  running_state_ = RUNNING;

  *closure = std::move(tasks_.front().closure);
  uint32_t order_num = tasks_.front().order_num;
  tasks_.pop_front();

  return order_num;
}

void Scheduler::Sequence::FinishTask() {
  DCHECK_EQ(running_state_, RUNNING);
  running_state_ = IDLE;
}

void Scheduler::Sequence::AddWaitFence(const SyncToken& sync_token,
                                       uint32_t order_num) {
  wait_fences_.push_back({sync_token, order_num});
}

void Scheduler::Sequence::RemoveWaitFence(const SyncToken& sync_token,
                                          uint32_t order_num) {
  base::Erase(wait_fences_, Fence{sync_token, order_num});
}

void Scheduler::Sequence::AddReleaseFence(const SyncToken& sync_token,
                                          uint32_t order_num) {
  release_fences_.push_back({sync_token, order_num});
}

void Scheduler::Sequence::RemoveReleaseFence(const SyncToken& sync_token,
                                             uint32_t order_num) {
  base::Erase(release_fences_, Fence{sync_token, order_num});
}

void Scheduler::Sequence::AddClientWait(CommandBufferId command_buffer_id) {
  client_waits_.insert(command_buffer_id);
}

void Scheduler::Sequence::RemoveClientWait(CommandBufferId command_buffer_id) {
  client_waits_.erase(command_buffer_id);
}

Scheduler::Scheduler(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     SyncPointManager* sync_point_manager)
    : task_runner_(std::move(task_runner)),
      sync_point_manager_(sync_point_manager),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

Scheduler::~Scheduler() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

SequenceId Scheduler::CreateSequence(SchedulingPriority priority) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  scoped_refptr<SyncPointOrderData> order_data =
      sync_point_manager_->CreateSyncPointOrderData();
  SequenceId sequence_id = order_data->sequence_id();
  auto sequence =
      std::make_unique<Sequence>(sequence_id, priority, std::move(order_data));
  sequences_.emplace(sequence_id, std::move(sequence));
  return sequence_id;
}

void Scheduler::DestroySequence(SequenceId sequence_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  if (sequence->scheduled())
    rebuild_scheduling_queue_ = true;

  sequences_.erase(sequence_id);
}

Scheduler::Sequence* Scheduler::GetSequence(SequenceId sequence_id) {
  lock_.AssertAcquired();
  auto it = sequences_.find(sequence_id);
  if (it != sequences_.end())
    return it->second.get();
  return nullptr;
}

void Scheduler::EnableSequence(SequenceId sequence_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(true);
  TryScheduleSequence(sequence);
}

void Scheduler::DisableSequence(SequenceId sequence_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->SetEnabled(false);
}

void Scheduler::RaisePriorityForClientWait(SequenceId sequence_id,
                                           CommandBufferId command_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->AddClientWait(command_buffer_id);
  TryScheduleSequence(sequence);
}

void Scheduler::ResetPriorityForClientWait(SequenceId sequence_id,
                                           CommandBufferId command_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->RemoveClientWait(command_buffer_id);
  TryScheduleSequence(sequence);
}

void Scheduler::ScheduleTask(Task task) {
  base::AutoLock auto_lock(lock_);
  ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTasks(std::vector<Task> tasks) {
  base::AutoLock auto_lock(lock_);
  for (auto& task : tasks)
    ScheduleTaskHelper(std::move(task));
}

void Scheduler::ScheduleTaskHelper(Task task) {
  lock_.AssertAcquired();
  SequenceId sequence_id = task.sequence_id;
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);

  uint32_t order_num = sequence->ScheduleTask(std::move(task.closure));

  for (const SyncToken& sync_token : task.sync_token_fences) {
    SequenceId release_id =
        sync_point_manager_->GetSyncTokenReleaseSequenceId(sync_token);
    Sequence* release_sequence = GetSequence(release_id);
    if (!release_sequence)
      continue;
    if (sync_point_manager_->Wait(
            sync_token, sequence_id, order_num,
            base::Bind(&Scheduler::SyncTokenFenceReleased,
                       weak_factory_.GetWeakPtr(), sync_token, order_num,
                       release_id, sequence_id))) {
      sequence->AddWaitFence(sync_token, order_num);
      release_sequence->AddReleaseFence(sync_token, order_num);
      TryScheduleSequence(release_sequence);
    }
  }

  TryScheduleSequence(sequence);
}

void Scheduler::ContinueTask(SequenceId sequence_id,
                             base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(sequence_id);
  DCHECK(sequence);
  sequence->ContinueTask(std::move(closure));
}

bool Scheduler::ShouldYield(SequenceId sequence_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  RebuildSchedulingQueue();

  if (scheduling_queue_.empty())
    return false;

  Sequence* running_sequence = GetSequence(sequence_id);
  DCHECK(running_sequence);
  DCHECK(running_sequence->running());

  Sequence* next_sequence = GetSequence(scheduling_queue_.front().sequence_id);
  DCHECK(next_sequence);
  DCHECK(next_sequence->scheduled());

  return running_sequence->ShouldYieldTo(next_sequence);
}

void Scheduler::SyncTokenFenceReleased(const SyncToken& sync_token,
                                       uint32_t order_num,
                                       SequenceId release_sequence_id,
                                       SequenceId waiting_sequence_id) {
  base::AutoLock auto_lock(lock_);
  Sequence* sequence = GetSequence(waiting_sequence_id);
  if (sequence) {
    sequence->RemoveWaitFence(sync_token, order_num);
    TryScheduleSequence(sequence);
  }
  Sequence* release_sequence = GetSequence(release_sequence_id);
  if (release_sequence) {
    release_sequence->RemoveReleaseFence(sync_token, order_num);
    TryScheduleSequence(release_sequence);
  }
}

void Scheduler::TryScheduleSequence(Sequence* sequence) {
  lock_.AssertAcquired();

  if (sequence->running()) {
    // Update priority of running sequence because of sync token releases.
    DCHECK(running_);
    sequence->UpdateRunningPriority();
  } else if (sequence->NeedsRescheduling()) {
    // Rebuild scheduling queue if priority changed for a scheduled sequence.
    DCHECK(running_);
    DCHECK(sequence->IsRunnable());
    rebuild_scheduling_queue_ = true;
  } else if (!sequence->scheduled() && sequence->IsRunnable()) {
    // Insert into scheduling queue if sequence isn't already scheduled.
    SchedulingState scheduling_state = sequence->SetScheduled();
    scheduling_queue_.push_back(scheduling_state);
    std::push_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                   &SchedulingState::Comparator);
    if (!running_) {
      TRACE_EVENT_ASYNC_BEGIN0("gpu", "Scheduler::Running", this);
      running_ = true;
      task_runner_->PostTask(FROM_HERE, base::Bind(&Scheduler::RunNextTask,
                                                   weak_factory_.GetWeakPtr()));
    }
  }
}

void Scheduler::RebuildSchedulingQueue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  lock_.AssertAcquired();

  if (!rebuild_scheduling_queue_)
    return;
  rebuild_scheduling_queue_ = false;

  scheduling_queue_.clear();
  for (const auto& kv : sequences_) {
    Sequence* sequence = kv.second.get();
    if (!sequence->IsRunnable() || sequence->running())
      continue;
    SchedulingState scheduling_state = sequence->SetScheduled();
    scheduling_queue_.push_back(scheduling_state);
  }

  std::make_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                 &SchedulingState::Comparator);
}

void Scheduler::RunNextTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);

  RebuildSchedulingQueue();

  if (scheduling_queue_.empty()) {
    TRACE_EVENT_ASYNC_END0("gpu", "Scheduler::Running", this);
    running_ = false;
    return;
  }

  std::pop_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                &SchedulingState::Comparator);
  SchedulingState state = scheduling_queue_.back();
  scheduling_queue_.pop_back();

  TRACE_EVENT1("gpu", "Scheduler::RunNextTask", "state", state.AsValue());

  Sequence* sequence = GetSequence(state.sequence_id);
  DCHECK(sequence);

  base::OnceClosure closure;
  uint32_t order_num = sequence->BeginTask(&closure);
  DCHECK_EQ(order_num, state.order_num);

  // Begin/FinishProcessingOrderNumber must be called with the lock released
  // because they can renter the scheduler in Enable/DisableSequence.
  scoped_refptr<SyncPointOrderData> order_data = sequence->order_data();
  {
    base::AutoUnlock auto_unlock(lock_);
    order_data->BeginProcessingOrderNumber(order_num);
    std::move(closure).Run();
    if (order_data->IsProcessingOrderNumber())
      order_data->FinishProcessingOrderNumber(order_num);
  }

  // Check if sequence hasn't been destroyed.
  sequence = GetSequence(state.sequence_id);
  if (sequence) {
    sequence->FinishTask();
    if (sequence->IsRunnable()) {
      SchedulingState scheduling_state = sequence->SetScheduled();
      scheduling_queue_.push_back(scheduling_state);
      std::push_heap(scheduling_queue_.begin(), scheduling_queue_.end(),
                     &SchedulingState::Comparator);
    }
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(&Scheduler::RunNextTask,
                                               weak_factory_.GetWeakPtr()));
}

}  // namespace gpu
