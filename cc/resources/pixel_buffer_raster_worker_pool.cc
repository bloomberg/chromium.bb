// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/pixel_buffer_raster_worker_pool.h"

#include "base/containers/stack_container.h"
#include "base/debug/trace_event.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource.h"

namespace cc {
namespace {

const int kCheckForCompletedRasterTasksDelayMs = 6;

const size_t kMaxScheduledRasterTasks = 48;

typedef base::StackVector<internal::WorkerPoolTask*, kMaxScheduledRasterTasks>
    WorkerPoolTaskVector;

// Only used as std::find_if predicate for DCHECKs.
bool WasCanceled(const internal::WorkerPoolTask* task) {
  return !task->HasFinishedRunning();
}

}  // namespace

// static
scoped_ptr<RasterWorkerPool> PixelBufferRasterWorkerPool::Create(
    ResourceProvider* resource_provider,
    size_t max_transfer_buffer_usage_bytes) {
  return make_scoped_ptr<RasterWorkerPool>(
      new PixelBufferRasterWorkerPool(GetTaskGraphRunner(),
                                      resource_provider,
                                      max_transfer_buffer_usage_bytes));
}

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    internal::TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    size_t max_transfer_buffer_usage_bytes)
    : RasterWorkerPool(task_graph_runner, resource_provider),
      shutdown_(false),
      scheduled_raster_task_count_(0),
      bytes_pending_upload_(0),
      max_bytes_pending_upload_(max_transfer_buffer_usage_bytes),
      has_performed_uploads_since_last_flush_(false),
      check_for_completed_raster_tasks_pending_(false),
      should_notify_client_if_no_tasks_are_pending_(false),
      should_notify_client_if_no_tasks_required_for_activation_are_pending_(
          false),
      raster_finished_task_pending_(false),
      raster_required_for_activation_finished_task_pending_(false),
      weak_factory_(this) {}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK(shutdown_);
  DCHECK(!check_for_completed_raster_tasks_pending_);
  DCHECK_EQ(0u, raster_task_states_.size());
  DCHECK_EQ(0u, raster_tasks_with_pending_upload_.size());
  DCHECK_EQ(0u, completed_raster_tasks_.size());
  DCHECK_EQ(0u, completed_image_decode_tasks_.size());
}

void PixelBufferRasterWorkerPool::Shutdown() {
  shutdown_ = true;
  RasterWorkerPool::Shutdown();

  CheckForCompletedWorkerPoolTasks();
  CheckForCompletedUploads();

  weak_factory_.InvalidateWeakPtrs();
  check_for_completed_raster_tasks_pending_ = false;

  for (RasterTaskStateMap::iterator it = raster_task_states_.begin();
       it != raster_task_states_.end();
       ++it) {
    internal::WorkerPoolTask* task = it->first;
    RasterTaskState& state = it->second;

    // All unscheduled tasks need to be canceled.
    if (state.type == RasterTaskState::UNSCHEDULED) {
      completed_raster_tasks_.push_back(task);
      state.type = RasterTaskState::COMPLETED;
    }
  }
  DCHECK_EQ(completed_raster_tasks_.size(), raster_task_states_.size());
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

  if (!should_notify_client_if_no_tasks_are_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  should_notify_client_if_no_tasks_are_pending_ = true;
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ = true;

  raster_tasks_required_for_activation_.clear();

  // Build new raster task state map.
  RasterTaskStateMap new_raster_task_states;
  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    internal::WorkerPoolTask* task = item.task;
    DCHECK(new_raster_task_states.find(task) == new_raster_task_states.end());

    RasterTaskStateMap::iterator state_it = raster_task_states_.find(task);
    if (state_it != raster_task_states_.end()) {
      RasterTaskState& state = state_it->second;

      new_raster_task_states[task] = state;
      // |raster_tasks_required_for_activation_| contains all tasks that need to
      // complete before we can send a "ready to activate" signal. Tasks that
      // have already completed should not be part of this set.
      if (state.type != RasterTaskState::COMPLETED &&
          item.required_for_activation)
        raster_tasks_required_for_activation_.insert(task);

      raster_task_states_.erase(state_it);
    } else {
      DCHECK(!task->HasBeenScheduled());
      new_raster_task_states[task].type = RasterTaskState::UNSCHEDULED;
      if (item.required_for_activation)
        raster_tasks_required_for_activation_.insert(task);
    }
  }

  // Transfer old raster task state to |new_raster_task_states| and cancel all
  // remaining unscheduled tasks.
  for (RasterTaskStateMap::iterator it = raster_task_states_.begin();
       it != raster_task_states_.end();
       ++it) {
    internal::WorkerPoolTask* task = it->first;
    RasterTaskState& state = it->second;
    DCHECK(new_raster_task_states.find(task) == new_raster_task_states.end());

    // Unscheduled task can be canceled.
    if (state.type == RasterTaskState::UNSCHEDULED) {
      DCHECK(!task->HasBeenScheduled());
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(task);
      new_raster_task_states[task].type = RasterTaskState::COMPLETED;
      continue;
    }

    // Move state to |new_raster_task_states|.
    new_raster_task_states[task] = state;
  }

  raster_tasks_.Swap(queue);
  raster_task_states_.swap(new_raster_task_states);

  // Check for completed tasks when ScheduleTasks() is called as
  // priorities might have changed and this maximizes the number
  // of top priority tasks that are scheduled.
  CheckForCompletedWorkerPoolTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Schedule new tasks.
  ScheduleMoreTasks();

  // Cancel any pending check for completed raster tasks and schedule
  // another check.
  check_for_completed_raster_tasks_time_ = base::TimeTicks();
  ScheduleCheckForCompletedRasterTasks();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      StateName(),
      "state",
      TracedValue::FromValue(StateAsValue().release()));
}

unsigned PixelBufferRasterWorkerPool::GetResourceTarget() const {
  return GL_TEXTURE_2D;
}

ResourceFormat PixelBufferRasterWorkerPool::GetResourceFormat() const {
  return resource_provider()->memory_efficient_texture_format();
}

void PixelBufferRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::CheckForCompletedTasks");

  CheckForCompletedWorkerPoolTasks();
  CheckForCompletedUploads();
  FlushUploads();

  while (!completed_image_decode_tasks_.empty()) {
    internal::WorkerPoolTask* task =
        completed_image_decode_tasks_.front().get();

    task->RunReplyOnOriginThread();

    completed_image_decode_tasks_.pop_front();
  }

  while (!completed_raster_tasks_.empty()) {
    internal::WorkerPoolTask* task = completed_raster_tasks_.front().get();
    DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
    DCHECK_EQ(RasterTaskState::COMPLETED, raster_task_states_[task].type);

    raster_task_states_.erase(task);

    task->RunReplyOnOriginThread();

    completed_raster_tasks_.pop_front();
  }
}

SkCanvas* PixelBufferRasterWorkerPool::AcquireCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
  DCHECK(!raster_task_states_[task].resource);
  raster_task_states_[task].resource = resource;
  resource_provider()->AcquirePixelRasterBuffer(resource->id());
  return resource_provider()->MapPixelRasterBuffer(resource->id());
}

void PixelBufferRasterWorkerPool::ReleaseCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
  DCHECK(raster_task_states_[task].resource == resource);
  resource_provider()->ReleasePixelRasterBuffer(resource->id());
}

void PixelBufferRasterWorkerPool::OnRasterTasksFinished() {
  // |should_notify_client_if_no_tasks_are_pending_| can be set to false as
  // a result of a scheduled CheckForCompletedRasterTasks() call. No need to
  // perform another check in that case as we've already notified the client.
  if (!should_notify_client_if_no_tasks_are_pending_)
    return;
  raster_finished_task_pending_ = false;

  // Call CheckForCompletedRasterTasks() when we've finished running all
  // raster tasks needed since last time ScheduleTasks() was called.
  // This reduces latency between the time when all tasks have finished
  // running and the time when the client is notified.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::OnRasterTasksRequiredForActivationFinished() {
  // Analogous to OnRasterTasksFinished(), there's no need to call
  // CheckForCompletedRasterTasks() if the client has already been notified.
  if (!should_notify_client_if_no_tasks_required_for_activation_are_pending_)
    return;
  raster_required_for_activation_finished_task_pending_ = false;

  // This reduces latency between the time when all tasks required for
  // activation have finished running and the time when the client is
  // notified.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::FlushUploads() {
  if (!has_performed_uploads_since_last_flush_)
    return;

  resource_provider()->ShallowFlushIfSupported();
  has_performed_uploads_since_last_flush_ = false;
}

void PixelBufferRasterWorkerPool::CheckForCompletedUploads() {
  TaskDeque tasks_with_completed_uploads;

  // First check if any have completed.
  while (!raster_tasks_with_pending_upload_.empty()) {
    internal::WorkerPoolTask* task =
        raster_tasks_with_pending_upload_.front().get();
    DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
    RasterTaskState& state = raster_task_states_[task];
    DCHECK_EQ(RasterTaskState::UPLOADING, state.type);

    // Uploads complete in the order they are issued.
    if (!resource_provider()->DidSetPixelsComplete(state.resource->id()))
      break;

    tasks_with_completed_uploads.push_back(task);
    raster_tasks_with_pending_upload_.pop_front();
  }

  DCHECK(client());
  bool should_force_some_uploads_to_complete =
      shutdown_ || client()->ShouldForceTasksRequiredForActivationToComplete();

  if (should_force_some_uploads_to_complete) {
    TaskDeque tasks_with_uploads_to_force;
    TaskDeque::iterator it = raster_tasks_with_pending_upload_.begin();
    while (it != raster_tasks_with_pending_upload_.end()) {
      internal::WorkerPoolTask* task = it->get();
      DCHECK(raster_task_states_.find(task) != raster_task_states_.end());

      // Force all uploads required for activation to complete.
      // During shutdown, force all pending uploads to complete.
      if (shutdown_ || IsRasterTaskRequiredForActivation(task)) {
        tasks_with_uploads_to_force.push_back(task);
        tasks_with_completed_uploads.push_back(task);
        it = raster_tasks_with_pending_upload_.erase(it);
        continue;
      }

      ++it;
    }

    // Force uploads in reverse order. Since forcing can cause a wait on
    // all previous uploads, we would rather wait only once downstream.
    for (TaskDeque::reverse_iterator it = tasks_with_uploads_to_force.rbegin();
         it != tasks_with_uploads_to_force.rend();
         ++it) {
      internal::WorkerPoolTask* task = it->get();
      RasterTaskState& state = raster_task_states_[task];
      DCHECK(state.resource);

      resource_provider()->ForceSetPixelsToComplete(state.resource->id());
      has_performed_uploads_since_last_flush_ = true;
    }
  }

  // Release shared memory and move tasks with completed uploads
  // to |completed_raster_tasks_|.
  while (!tasks_with_completed_uploads.empty()) {
    internal::WorkerPoolTask* task = tasks_with_completed_uploads.front().get();
    RasterTaskState& state = raster_task_states_[task];

    bytes_pending_upload_ -= state.resource->bytes();

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    DCHECK(std::find(completed_raster_tasks_.begin(),
                     completed_raster_tasks_.end(),
                     task) == completed_raster_tasks_.end());
    completed_raster_tasks_.push_back(task);
    state.type = RasterTaskState::COMPLETED;

    raster_tasks_required_for_activation_.erase(task);

    tasks_with_completed_uploads.pop_front();
  }
}

void PixelBufferRasterWorkerPool::ScheduleCheckForCompletedRasterTasks() {
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(kCheckForCompletedRasterTasksDelayMs);
  if (check_for_completed_raster_tasks_time_.is_null())
    check_for_completed_raster_tasks_time_ = base::TimeTicks::Now() + delay;

  if (check_for_completed_raster_tasks_pending_)
    return;

  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PixelBufferRasterWorkerPool::OnCheckForCompletedRasterTasks,
                 weak_factory_.GetWeakPtr()),
      delay);
  check_for_completed_raster_tasks_pending_ = true;
}

void PixelBufferRasterWorkerPool::OnCheckForCompletedRasterTasks() {
  if (check_for_completed_raster_tasks_time_.is_null()) {
    check_for_completed_raster_tasks_pending_ = false;
    return;
  }

  base::TimeDelta delay =
      check_for_completed_raster_tasks_time_ - base::TimeTicks::Now();

  // Post another delayed task if it is not yet time to check for completed
  // raster tasks.
  if (delay > base::TimeDelta()) {
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PixelBufferRasterWorkerPool::OnCheckForCompletedRasterTasks,
                   weak_factory_.GetWeakPtr()),
        delay);
    return;
  }

  check_for_completed_raster_tasks_pending_ = false;
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks() {
  TRACE_EVENT0("cc",
               "PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks");

  DCHECK(should_notify_client_if_no_tasks_are_pending_);
  check_for_completed_raster_tasks_time_ = base::TimeTicks();

  CheckForCompletedWorkerPoolTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Determine what client notifications to generate.
  bool will_notify_client_that_no_tasks_required_for_activation_are_pending =
      (should_notify_client_if_no_tasks_required_for_activation_are_pending_ &&
       !raster_required_for_activation_finished_task_pending_ &&
       !HasPendingTasksRequiredForActivation());
  bool will_notify_client_that_no_tasks_are_pending =
      (should_notify_client_if_no_tasks_are_pending_ &&
       !raster_required_for_activation_finished_task_pending_ &&
       !raster_finished_task_pending_ && !HasPendingTasks());

  // Adjust the need to generate notifications before scheduling more tasks.
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ &=
      !will_notify_client_that_no_tasks_required_for_activation_are_pending;
  should_notify_client_if_no_tasks_are_pending_ &=
      !will_notify_client_that_no_tasks_are_pending;

  scheduled_raster_task_count_ = 0;
  if (PendingRasterTaskCount())
    ScheduleMoreTasks();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      StateName(),
      "state",
      TracedValue::FromValue(StateAsValue().release()));

  // Schedule another check for completed raster tasks while there are
  // pending raster tasks or pending uploads.
  if (HasPendingTasks())
    ScheduleCheckForCompletedRasterTasks();

  // Generate client notifications.
  if (will_notify_client_that_no_tasks_required_for_activation_are_pending) {
    DCHECK(std::find_if(raster_tasks_required_for_activation_.begin(),
                        raster_tasks_required_for_activation_.end(),
                        WasCanceled) ==
           raster_tasks_required_for_activation_.end());
    client()->DidFinishRunningTasksRequiredForActivation();
  }
  if (will_notify_client_that_no_tasks_are_pending) {
    TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
    DCHECK(!HasPendingTasksRequiredForActivation());
    client()->DidFinishRunningTasks();
  }
}

void PixelBufferRasterWorkerPool::ScheduleMoreTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleMoreTasks");

  WorkerPoolTaskVector tasks;
  WorkerPoolTaskVector tasks_required_for_activation;

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  size_t bytes_pending_upload = bytes_pending_upload_;
  bool did_throttle_raster_tasks = false;

  for (RasterTaskQueue::Item::Vector::const_iterator it =
           raster_tasks_.items.begin();
       it != raster_tasks_.items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    internal::RasterWorkerPoolTask* task = item.task;

    // |raster_task_states_| contains the state of all tasks that we have not
    // yet run reply callbacks for.
    RasterTaskStateMap::iterator state_it = raster_task_states_.find(task);
    if (state_it == raster_task_states_.end())
      continue;

    RasterTaskState& state = state_it->second;

    // Skip task if completed.
    if (state.type == RasterTaskState::COMPLETED) {
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) != completed_raster_tasks_.end());
      continue;
    }

    // All raster tasks need to be throttled by bytes of pending uploads.
    size_t new_bytes_pending_upload = bytes_pending_upload;
    new_bytes_pending_upload += task->resource()->bytes();
    if (new_bytes_pending_upload > max_bytes_pending_upload_) {
      did_throttle_raster_tasks = true;
      break;
    }

    // If raster has finished, just update |bytes_pending_upload|.
    if (state.type == RasterTaskState::UPLOADING) {
      DCHECK(!task->HasCompleted());
      bytes_pending_upload = new_bytes_pending_upload;
      continue;
    }

    // Throttle raster tasks based on kMaxScheduledRasterTasks.
    if (tasks.container().size() >= kMaxScheduledRasterTasks) {
      did_throttle_raster_tasks = true;
      break;
    }

    // Update |bytes_pending_upload| now that task has cleared all
    // throttling limits.
    bytes_pending_upload = new_bytes_pending_upload;

    DCHECK(state.type == RasterTaskState::UNSCHEDULED ||
           state.type == RasterTaskState::SCHEDULED);
    state.type = RasterTaskState::SCHEDULED;

    InsertNodeForRasterTask(&graph_, task, task->dependencies(), priority++);

    tasks.container().push_back(task);
    if (item.required_for_activation)
      tasks_required_for_activation.container().push_back(task);
  }

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task;

  size_t scheduled_raster_task_required_for_activation_count =
      tasks_required_for_activation.container().size();
  DCHECK_LE(scheduled_raster_task_required_for_activation_count,
            raster_tasks_required_for_activation_.size());
  // Schedule OnRasterTasksRequiredForActivationFinished call only when
  // notification is pending and throttling is not preventing all pending
  // tasks required for activation from being scheduled.
  if (scheduled_raster_task_required_for_activation_count ==
          raster_tasks_required_for_activation_.size() &&
      should_notify_client_if_no_tasks_required_for_activation_are_pending_) {
    new_raster_required_for_activation_finished_task =
        CreateRasterRequiredForActivationFinishedTask(
            raster_tasks_required_for_activation_.size());
    raster_required_for_activation_finished_task_pending_ = true;
    InsertNodeForTask(&graph_,
                      new_raster_required_for_activation_finished_task.get(),
                      kRasterRequiredForActivationFinishedTaskPriority,
                      scheduled_raster_task_required_for_activation_count);
    for (WorkerPoolTaskVector::ContainerType::const_iterator it =
             tasks_required_for_activation.container().begin();
         it != tasks_required_for_activation.container().end();
         ++it) {
      graph_.edges.push_back(internal::TaskGraph::Edge(
          *it, new_raster_required_for_activation_finished_task.get()));
    }
  }

  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task;

  size_t scheduled_raster_task_count = tasks.container().size();
  DCHECK_LE(scheduled_raster_task_count, PendingRasterTaskCount());
  // Schedule OnRasterTasksFinished call only when notification is pending
  // and throttling is not preventing all pending tasks from being scheduled.
  if (!did_throttle_raster_tasks &&
      should_notify_client_if_no_tasks_are_pending_) {
    new_raster_finished_task = CreateRasterFinishedTask();
    raster_finished_task_pending_ = true;
    InsertNodeForTask(&graph_,
                      new_raster_finished_task.get(),
                      kRasterFinishedTaskPriority,
                      scheduled_raster_task_count);
    for (WorkerPoolTaskVector::ContainerType::const_iterator it =
             tasks.container().begin();
         it != tasks.container().end();
         ++it) {
      graph_.edges.push_back(
          internal::TaskGraph::Edge(*it, new_raster_finished_task.get()));
    }
  }

  SetTaskGraph(&graph_);

  scheduled_raster_task_count_ = scheduled_raster_task_count;

  set_raster_finished_task(new_raster_finished_task);
  set_raster_required_for_activation_finished_task(
      new_raster_required_for_activation_finished_task);
}

unsigned PixelBufferRasterWorkerPool::PendingRasterTaskCount() const {
  unsigned num_completed_raster_tasks =
      raster_tasks_with_pending_upload_.size() + completed_raster_tasks_.size();
  DCHECK_GE(raster_task_states_.size(), num_completed_raster_tasks);
  return raster_task_states_.size() - num_completed_raster_tasks;
}

bool PixelBufferRasterWorkerPool::HasPendingTasks() const {
  return PendingRasterTaskCount() || !raster_tasks_with_pending_upload_.empty();
}

bool PixelBufferRasterWorkerPool::HasPendingTasksRequiredForActivation() const {
  return !raster_tasks_required_for_activation_.empty();
}

const char* PixelBufferRasterWorkerPool::StateName() const {
  if (scheduled_raster_task_count_)
    return "rasterizing";
  if (PendingRasterTaskCount())
    return "throttled";
  if (!raster_tasks_with_pending_upload_.empty())
    return "waiting_for_uploads";

  return "finishing";
}

void PixelBufferRasterWorkerPool::CheckForCompletedWorkerPoolTasks() {
  CollectCompletedWorkerPoolTasks(&completed_tasks_);
  for (internal::Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(it->get());

    RasterTaskStateMap::iterator state_it = raster_task_states_.find(task);
    if (state_it == raster_task_states_.end()) {
      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      completed_image_decode_tasks_.push_back(task);
      continue;
    }

    RasterTaskState& state = state_it->second;
    DCHECK_EQ(RasterTaskState::SCHEDULED, state.type);
    DCHECK(state.resource);

    // Balanced with MapPixelRasterBuffer() call in AcquireCanvasForRaster().
    bool content_has_changed =
        resource_provider()->UnmapPixelRasterBuffer(state.resource->id());

    // |content_has_changed| can be false as result of task being canceled or
    // task implementation deciding not to modify bitmap (ie. analysis of raster
    // commands detected content as a solid color).
    if (!content_has_changed) {
      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      if (!task->HasFinishedRunning()) {
        // When priorites change, a raster task can be canceled as a result of
        // no longer being of high enough priority to fit in our throttled
        // raster task budget. The task has not yet completed in this case.
        RasterTaskQueue::Item::Vector::const_iterator item_it =
            std::find_if(raster_tasks_.items.begin(),
                         raster_tasks_.items.end(),
                         RasterTaskQueue::Item::TaskComparator(task));
        if (item_it != raster_tasks_.items.end()) {
          state.type = RasterTaskState::UNSCHEDULED;
          state.resource = NULL;
          continue;
        }
      }

      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(task);
      state.type = RasterTaskState::COMPLETED;
      raster_tasks_required_for_activation_.erase(task);
      continue;
    }

    DCHECK(task->HasFinishedRunning());

    resource_provider()->BeginSetPixels(state.resource->id());
    has_performed_uploads_since_last_flush_ = true;

    bytes_pending_upload_ += state.resource->bytes();
    raster_tasks_with_pending_upload_.push_back(task);
    state.type = RasterTaskState::UPLOADING;
  }
  completed_tasks_.clear();
}

bool PixelBufferRasterWorkerPool::IsRasterTaskRequiredForActivation(
    internal::WorkerPoolTask* task) const {
  return raster_tasks_required_for_activation_.find(task) !=
         raster_tasks_required_for_activation_.end();
}

scoped_ptr<base::Value> PixelBufferRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetInteger("completed_count", completed_raster_tasks_.size());
  state->SetInteger("pending_count", raster_task_states_.size());
  state->SetInteger("pending_upload_count",
                    raster_tasks_with_pending_upload_.size());
  state->SetInteger("pending_required_for_activation_count",
                    raster_tasks_required_for_activation_.size());
  state->Set("throttle_state", ThrottleStateAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> PixelBufferRasterWorkerPool::ThrottleStateAsValue()
    const {
  scoped_ptr<base::DictionaryValue> throttle_state(new base::DictionaryValue);

  throttle_state->SetInteger("bytes_available_for_upload",
                             max_bytes_pending_upload_ - bytes_pending_upload_);
  throttle_state->SetInteger("bytes_pending_upload", bytes_pending_upload_);
  throttle_state->SetInteger("scheduled_raster_task_count",
                             scheduled_raster_task_count_);
  return throttle_state.PassAs<base::Value>();
}

}  // namespace cc
