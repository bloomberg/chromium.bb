// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/pixel_buffer_raster_worker_pool.h"

#include "base/containers/stack_container.h"
#include "base/debug/trace_event.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#endif

namespace cc {
namespace {

const int kCheckForCompletedRasterTasksDelayMs = 6;

const size_t kMaxScheduledRasterTasks = 48;

typedef base::StackVector<internal::GraphNode*, kMaxScheduledRasterTasks>
    NodeVector;

void AddDependenciesToGraphNode(internal::GraphNode* node,
                                const NodeVector::ContainerType& dependencies) {
  for (NodeVector::ContainerType::const_iterator it = dependencies.begin();
       it != dependencies.end();
       ++it) {
    internal::GraphNode* dependency = *it;

    node->add_dependency();
    dependency->add_dependent(node);
  }
}

// Only used as std::find_if predicate for DCHECKs.
bool WasCanceled(const internal::RasterWorkerPoolTask* task) {
  return !task->HasFinishedRunning();
}

}  // namespace

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    ResourceProvider* resource_provider,
    ContextProvider* context_provider,
    size_t max_transfer_buffer_usage_bytes)
    : RasterWorkerPool(resource_provider, context_provider),
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
      raster_required_for_activation_finished_task_pending_(false) {}

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
  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;
  for (RasterTaskStateMap::iterator it = raster_task_states_.begin();
       it != raster_task_states_.end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->first;

    // All unscheduled tasks need to be canceled.
    if (it->second == UNSCHEDULED) {
      completed_raster_tasks_.push_back(task);
      it->second = COMPLETED;
    }
  }
  DCHECK_EQ(completed_raster_tasks_.size(), raster_task_states_.size());
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  if (!should_notify_client_if_no_tasks_are_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  should_notify_client_if_no_tasks_are_pending_ = true;
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ = true;

  raster_tasks_required_for_activation_.clear();

  // Build new raster task state map.
  RasterTaskStateMap new_raster_task_states;
  RasterTaskVector gpu_raster_tasks;
  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(new_raster_task_states.find(task) == new_raster_task_states.end());

    if (task->use_gpu_rasterization()) {
      gpu_raster_tasks.push_back(task);
      continue;
    }

    RasterTaskStateMap::iterator state_it = raster_task_states_.find(task);
    if (state_it != raster_task_states_.end()) {
      RasterTaskState state = state_it->second;

      new_raster_task_states[task] = state;
      // |raster_tasks_required_for_activation_| contains all tasks that need to
      // complete before we can send a "ready to activate" signal. Tasks that
      // have already completed should not be part of this set.
      if (state != COMPLETED && IsRasterTaskRequiredForActivation(task))
        raster_tasks_required_for_activation_.insert(task);

      raster_task_states_.erase(state_it);
    } else {
      new_raster_task_states[task] = UNSCHEDULED;
      if (IsRasterTaskRequiredForActivation(task))
        raster_tasks_required_for_activation_.insert(task);
    }
  }

  // Transfer old raster task state to |new_raster_task_states| and cancel all
  // remaining unscheduled tasks.
  for (RasterTaskStateMap::iterator it = raster_task_states_.begin();
       it != raster_task_states_.end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    DCHECK(new_raster_task_states.find(task) == new_raster_task_states.end());
    DCHECK(!IsRasterTaskRequiredForActivation(task));

    // Unscheduled task can be canceled.
    if (it->second == UNSCHEDULED) {
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(task);
      new_raster_task_states[task] = COMPLETED;
      continue;
    }

    // Move state to |new_raster_task_states|.
    new_raster_task_states[task] = it->second;
  }

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
  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;
  ScheduleCheckForCompletedRasterTasks();

  RunGpuRasterTasks(gpu_raster_tasks);

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
    internal::RasterWorkerPoolTask* task =
        completed_raster_tasks_.front().get();
    DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
    DCHECK_EQ(COMPLETED, raster_task_states_[task]);

    raster_task_states_.erase(task);

    task->RunReplyOnOriginThread();

    completed_raster_tasks_.pop_front();
  }

  CheckForCompletedGpuRasterTasks();
}

void* PixelBufferRasterWorkerPool::AcquireBufferForRaster(
    internal::RasterWorkerPoolTask* task,
    int* stride) {
  // Request a pixel buffer. This will reserve shared memory.
  resource_provider()->AcquirePixelBuffer(task->resource()->id());

  *stride = 0;
  return resource_provider()->MapPixelBuffer(task->resource()->id());
}

void PixelBufferRasterWorkerPool::OnRasterCompleted(
    internal::RasterWorkerPoolTask* task,
    const PicturePileImpl::Analysis& analysis) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc"),
               "PixelBufferRasterWorkerPool::OnRasterCompleted",
               "was_canceled",
               !task->HasFinishedRunning(),
               "needs_upload",
               task->HasFinishedRunning() && !analysis.is_solid_color);

  DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
  DCHECK_EQ(SCHEDULED, raster_task_states_[task]);

  // Balanced with MapPixelBuffer() call in AcquireBufferForRaster().
  resource_provider()->UnmapPixelBuffer(task->resource()->id());

  if (!task->HasFinishedRunning() || analysis.is_solid_color) {
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    if (!task->HasFinishedRunning()) {
      // When priorites change, a raster task can be canceled as a result of
      // no longer being of high enough priority to fit in our throttled
      // raster task budget. The task has not yet completed in this case.
      RasterTaskVector::const_iterator it =
          std::find(raster_tasks().begin(), raster_tasks().end(), task);
      if (it != raster_tasks().end()) {
        raster_task_states_[task] = UNSCHEDULED;
        return;
      }
    }

    DCHECK(std::find(completed_raster_tasks_.begin(),
                     completed_raster_tasks_.end(),
                     task) == completed_raster_tasks_.end());
    completed_raster_tasks_.push_back(task);
    raster_task_states_[task] = COMPLETED;
    raster_tasks_required_for_activation_.erase(task);
    return;
  }

  DCHECK(task->HasFinishedRunning());

  resource_provider()->BeginSetPixels(task->resource()->id());
  has_performed_uploads_since_last_flush_ = true;

  bytes_pending_upload_ += task->resource()->bytes();
  raster_tasks_with_pending_upload_.push_back(task);
  raster_task_states_[task] = UPLOADING;
}

void PixelBufferRasterWorkerPool::OnImageDecodeCompleted(
    internal::WorkerPoolTask* task) {
  completed_image_decode_tasks_.push_back(task);
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
  RasterTaskDeque tasks_with_completed_uploads;

  // First check if any have completed.
  while (!raster_tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task =
        raster_tasks_with_pending_upload_.front().get();
    DCHECK(raster_task_states_.find(task) != raster_task_states_.end());
    DCHECK_EQ(UPLOADING, raster_task_states_[task]);

    // Uploads complete in the order they are issued.
    if (!resource_provider()->DidSetPixelsComplete(task->resource()->id()))
      break;

    tasks_with_completed_uploads.push_back(task);
    raster_tasks_with_pending_upload_.pop_front();
  }

  DCHECK(client());
  bool should_force_some_uploads_to_complete =
      shutdown_ || client()->ShouldForceTasksRequiredForActivationToComplete();

  if (should_force_some_uploads_to_complete) {
    RasterTaskDeque tasks_with_uploads_to_force;
    RasterTaskDeque::iterator it = raster_tasks_with_pending_upload_.begin();
    while (it != raster_tasks_with_pending_upload_.end()) {
      internal::RasterWorkerPoolTask* task = it->get();
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
    for (RasterTaskDeque::reverse_iterator it =
             tasks_with_uploads_to_force.rbegin();
         it != tasks_with_uploads_to_force.rend();
         ++it) {
      resource_provider()->ForceSetPixelsToComplete((*it)->resource()->id());
      has_performed_uploads_since_last_flush_ = true;
    }
  }

  // Release shared memory and move tasks with completed uploads
  // to |completed_raster_tasks_|.
  while (!tasks_with_completed_uploads.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_completed_uploads.front().get();

    // It's now safe to release the pixel buffer and the shared memory.
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    DCHECK(std::find(completed_raster_tasks_.begin(),
                     completed_raster_tasks_.end(),
                     task) == completed_raster_tasks_.end());
    completed_raster_tasks_.push_back(task);
    raster_task_states_[task] = COMPLETED;

    raster_tasks_required_for_activation_.erase(task);

    tasks_with_completed_uploads.pop_front();
  }
}

void PixelBufferRasterWorkerPool::ScheduleCheckForCompletedRasterTasks() {
  if (check_for_completed_raster_tasks_pending_)
    return;

  check_for_completed_raster_tasks_callback_.Reset(
      base::Bind(&PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks,
                 base::Unretained(this)));
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      check_for_completed_raster_tasks_callback_.callback(),
      base::TimeDelta::FromMilliseconds(kCheckForCompletedRasterTasksDelayMs));
  check_for_completed_raster_tasks_pending_ = true;
}

void PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks() {
  TRACE_EVENT0("cc",
               "PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks");

  DCHECK(should_notify_client_if_no_tasks_are_pending_);

  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;

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
    DCHECK(std::find_if(raster_tasks_required_for_activation().begin(),
                        raster_tasks_required_for_activation().end(),
                        WasCanceled) ==
           raster_tasks_required_for_activation().end());
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

  enum RasterTaskType {
    PREPAINT_TYPE = 0,
    REQUIRED_FOR_ACTIVATION_TYPE = 1,
    NUM_TYPES = 2
  };
  NodeVector tasks[NUM_TYPES];
  unsigned priority = 2u;  // 0-1 reserved for RasterFinished tasks.
  TaskGraph graph;

  size_t bytes_pending_upload = bytes_pending_upload_;
  bool did_throttle_raster_tasks = false;

  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->get();

    // |raster_task_states_| contains the state of all tasks that we have not
    // yet run reply callbacks for.
    RasterTaskStateMap::iterator state_it = raster_task_states_.find(task);
    if (state_it == raster_task_states_.end())
      continue;

    // Skip task if completed.
    if (state_it->second == COMPLETED) {
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
    if (state_it->second == UPLOADING) {
      bytes_pending_upload = new_bytes_pending_upload;
      continue;
    }

    // Throttle raster tasks based on kMaxScheduledRasterTasks.
    size_t scheduled_raster_task_count =
        tasks[PREPAINT_TYPE].container().size() +
        tasks[REQUIRED_FOR_ACTIVATION_TYPE].container().size();
    if (scheduled_raster_task_count >= kMaxScheduledRasterTasks) {
      did_throttle_raster_tasks = true;
      break;
    }

    // Update |bytes_pending_upload| now that task has cleared all
    // throttling limits.
    bytes_pending_upload = new_bytes_pending_upload;

    RasterTaskType type = IsRasterTaskRequiredForActivation(task)
                              ? REQUIRED_FOR_ACTIVATION_TYPE
                              : PREPAINT_TYPE;

    task->ScheduleOnOriginThread(this);
    DCHECK(state_it->second == UNSCHEDULED || state_it->second == SCHEDULED);
    state_it->second = SCHEDULED;

    tasks[type].container().push_back(CreateGraphNodeForRasterTask(
        task, task->dependencies(), priority++, &graph));
  }

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task;

  size_t scheduled_raster_task_required_for_activation_count =
      tasks[REQUIRED_FOR_ACTIVATION_TYPE].container().size();
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
    internal::GraphNode* raster_required_for_activation_finished_node =
        CreateGraphNodeForTask(
            new_raster_required_for_activation_finished_task.get(),
            0u,  // Priority 0
            &graph);
    AddDependenciesToGraphNode(raster_required_for_activation_finished_node,
                               tasks[REQUIRED_FOR_ACTIVATION_TYPE].container());
  }

  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task;

  size_t scheduled_raster_task_count =
      tasks[PREPAINT_TYPE].container().size() +
      tasks[REQUIRED_FOR_ACTIVATION_TYPE].container().size();
  DCHECK_LE(scheduled_raster_task_count, PendingRasterTaskCount());
  // Schedule OnRasterTasksFinished call only when notification is pending
  // and throttling is not preventing all pending tasks from being scheduled.
  if (!did_throttle_raster_tasks &&
      should_notify_client_if_no_tasks_are_pending_) {
    new_raster_finished_task = CreateRasterFinishedTask();
    raster_finished_task_pending_ = true;
    internal::GraphNode* raster_finished_node =
        CreateGraphNodeForTask(new_raster_finished_task.get(),
                               1u,  // Priority 1
                               &graph);
    for (unsigned type = 0; type < NUM_TYPES; ++type) {
      AddDependenciesToGraphNode(raster_finished_node, tasks[type].container());
    }
  }

  SetTaskGraph(&graph);

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
  internal::Task::Vector completed_tasks;
  CollectCompletedWorkerPoolTasks(&completed_tasks);

  for (internal::Task::Vector::const_iterator it = completed_tasks.begin();
       it != completed_tasks.end();
       ++it) {
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();
  }
}

scoped_ptr<base::Value> PixelBufferRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetInteger("completed_count", completed_raster_tasks_.size());
  state->SetInteger("pending_count", raster_task_states_.size());
  state->SetInteger("pending_upload_count",
                    raster_tasks_with_pending_upload_.size());
  state->SetInteger("required_for_activation_count",
                    raster_tasks_required_for_activation_.size());
  state->Set("scheduled_state", ScheduledStateAsValue().release());
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
