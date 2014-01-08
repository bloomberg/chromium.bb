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

class PixelBufferWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(bool was_canceled, bool needs_upload)> Reply;

  PixelBufferWorkerPoolTaskImpl(internal::RasterWorkerPoolTask* task,
                                uint8_t* buffer,
                                const Reply& reply)
      : task_(task),
        buffer_(buffer),
        reply_(reply),
        needs_upload_(false) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    // |buffer_| can be NULL in lost context situations.
    if (!buffer_) {
      // |needs_upload_| still needs to be true as task has not
      // been canceled.
      needs_upload_ = true;
      return;
    }
    needs_upload_ = task_->RunOnWorkerThread(thread_index,
                                             buffer_,
                                             task_->resource()->size(),
                                             0);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    // |needs_upload_| must be be false if task didn't run.
    DCHECK(HasFinishedRunning() || !needs_upload_);
    reply_.Run(!HasFinishedRunning(), needs_upload_);
  }

 private:
  virtual ~PixelBufferWorkerPoolTaskImpl() {}

  scoped_refptr<internal::RasterWorkerPoolTask> task_;
  uint8_t* buffer_;
  const Reply reply_;
  bool needs_upload_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferWorkerPoolTaskImpl);
};

const int kCheckForCompletedRasterTasksDelayMs = 6;

const size_t kMaxScheduledRasterTasks = 48;

typedef base::StackVector<internal::GraphNode*,
                          kMaxScheduledRasterTasks> NodeVector;

void AddDependenciesToGraphNode(
    internal::GraphNode* node,
    const NodeVector::ContainerType& dependencies) {
  for (NodeVector::ContainerType::const_iterator it = dependencies.begin();
       it != dependencies.end(); ++it) {
    internal::GraphNode* dependency = *it;

    node->add_dependency();
    dependency->add_dependent(node);
  }
}

// Only used as std::find_if predicate for DCHECKs.
bool WasCanceled(const internal::RasterWorkerPoolTask* task) {
  return task->WasCanceled();
}

}  // namespace

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    ResourceProvider* resource_provider,
    size_t num_threads,
    size_t max_transfer_buffer_usage_bytes)
    : RasterWorkerPool(resource_provider, num_threads),
      shutdown_(false),
      scheduled_raster_task_count_(0),
      bytes_pending_upload_(0),
      max_bytes_pending_upload_(max_transfer_buffer_usage_bytes),
      has_performed_uploads_since_last_flush_(false),
      check_for_completed_raster_tasks_pending_(false),
      should_notify_client_if_no_tasks_are_pending_(false),
      should_notify_client_if_no_tasks_required_for_activation_are_pending_(
          false) {
}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK(shutdown_);
  DCHECK(!check_for_completed_raster_tasks_pending_);
  DCHECK_EQ(0u, pixel_buffer_tasks_.size());
  DCHECK_EQ(0u, tasks_with_pending_upload_.size());
  DCHECK_EQ(0u, completed_tasks_.size());
}

void PixelBufferRasterWorkerPool::Shutdown() {
  shutdown_ = true;
  RasterWorkerPool::Shutdown();
  RasterWorkerPool::CheckForCompletedTasks();
  CheckForCompletedUploads();
  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    internal::WorkerPoolTask* pixel_buffer_task = it->second.get();

    // All inactive tasks needs to be canceled.
    if (!pixel_buffer_task && !task->HasFinishedRunning()) {
      task->DidRun(true);
      completed_tasks_.push_back(task);
    }
  }
  DCHECK_EQ(completed_tasks_.size(), pixel_buffer_tasks_.size());
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  if (!should_notify_client_if_no_tasks_are_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  should_notify_client_if_no_tasks_are_pending_ = true;
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ = true;

  tasks_required_for_activation_.clear();

  // Build new pixel buffer task set.
  TaskMap new_pixel_buffer_tasks;
  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(new_pixel_buffer_tasks.find(task) == new_pixel_buffer_tasks.end());
    DCHECK(!task->HasCompleted());
    DCHECK(!task->WasCanceled());

    new_pixel_buffer_tasks[task] = pixel_buffer_tasks_[task];
    pixel_buffer_tasks_.erase(task);

    if (IsRasterTaskRequiredForActivation(task))
      tasks_required_for_activation_.insert(task);
  }

  // Transfer remaining pixel buffer tasks to |new_pixel_buffer_tasks|
  // and cancel all remaining inactive tasks.
  for (TaskMap::iterator it = pixel_buffer_tasks_.begin();
       it != pixel_buffer_tasks_.end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->first;
    internal::WorkerPoolTask* pixel_buffer_task = it->second.get();

    // Move task to |new_pixel_buffer_tasks|
    new_pixel_buffer_tasks[task] = pixel_buffer_task;

    // Inactive task can be canceled.
    if (!pixel_buffer_task && !task->HasFinishedRunning()) {
      task->DidRun(true);
      DCHECK(std::find(completed_tasks_.begin(),
                       completed_tasks_.end(),
                       task) == completed_tasks_.end());
      completed_tasks_.push_back(task);
    } else if (IsRasterTaskRequiredForActivation(task)) {
      tasks_required_for_activation_.insert(task);
    }
  }

  // |tasks_required_for_activation_| contains all tasks that need to
  // complete before we can send a "ready to activate" signal. Tasks
  // that have already completed should not be part of this set.
  for (TaskDeque::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end() && !tasks_required_for_activation_.empty();
       ++it) {
    tasks_required_for_activation_.erase(*it);
  }

  pixel_buffer_tasks_.swap(new_pixel_buffer_tasks);

  // Check for completed tasks when ScheduleTasks() is called as
  // priorities might have changed and this maximizes the number
  // of top priority tasks that are scheduled.
  RasterWorkerPool::CheckForCompletedTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Schedule new tasks.
  ScheduleMoreTasks();

  // Cancel any pending check for completed raster tasks and schedule
  // another check.
  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;
  ScheduleCheckForCompletedRasterTasks();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, StateName(),
      "state", TracedValue::FromValue(StateAsValue().release()));
}

GLenum PixelBufferRasterWorkerPool::GetResourceTarget() const {
  return GL_TEXTURE_2D;
}

ResourceFormat PixelBufferRasterWorkerPool::GetResourceFormat() const {
  return resource_provider()->memory_efficient_texture_format();
}

void PixelBufferRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::CheckForCompletedTasks");

  RasterWorkerPool::CheckForCompletedTasks();
  CheckForCompletedUploads();
  FlushUploads();

  TaskDeque completed_tasks;
  completed_tasks_.swap(completed_tasks);

  while (!completed_tasks.empty()) {
    internal::RasterWorkerPoolTask* task = completed_tasks.front().get();
    DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

    pixel_buffer_tasks_.erase(task);

    task->WillComplete();
    task->CompleteOnOriginThread();
    task->DidComplete();

    completed_tasks.pop_front();
  }
}

void PixelBufferRasterWorkerPool::OnRasterTasksFinished() {
  // |should_notify_client_if_no_tasks_are_pending_| can be set to false as
  // a result of a scheduled CheckForCompletedRasterTasks() call. No need to
  // perform another check in that case as we've already notified the client.
  if (!should_notify_client_if_no_tasks_are_pending_)
    return;

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
  while (!tasks_with_pending_upload_.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_pending_upload_.front().get();

    // Uploads complete in the order they are issued.
    if (!resource_provider()->DidSetPixelsComplete(task->resource()->id()))
      break;

    tasks_with_completed_uploads.push_back(task);
    tasks_with_pending_upload_.pop_front();
  }

  DCHECK(client());
  bool should_force_some_uploads_to_complete =
      shutdown_ || client()->ShouldForceTasksRequiredForActivationToComplete();

  if (should_force_some_uploads_to_complete) {
    TaskDeque tasks_with_uploads_to_force;
    TaskDeque::iterator it = tasks_with_pending_upload_.begin();
    while (it != tasks_with_pending_upload_.end()) {
      internal::RasterWorkerPoolTask* task = it->get();
      DCHECK(pixel_buffer_tasks_.find(task) != pixel_buffer_tasks_.end());

      // Force all uploads required for activation to complete.
      // During shutdown, force all pending uploads to complete.
      if (shutdown_ || IsRasterTaskRequiredForActivation(task)) {
        tasks_with_uploads_to_force.push_back(task);
        tasks_with_completed_uploads.push_back(task);
        it = tasks_with_pending_upload_.erase(it);
        continue;
      }

      ++it;
    }

    // Force uploads in reverse order. Since forcing can cause a wait on
    // all previous uploads, we would rather wait only once downstream.
    for (TaskDeque::reverse_iterator it = tasks_with_uploads_to_force.rbegin();
         it != tasks_with_uploads_to_force.rend();
         ++it) {
      resource_provider()->ForceSetPixelsToComplete((*it)->resource()->id());
      has_performed_uploads_since_last_flush_ = true;
    }
  }

  // Release shared memory and move tasks with completed uploads
  // to |completed_tasks_|.
  while (!tasks_with_completed_uploads.empty()) {
    internal::RasterWorkerPoolTask* task =
        tasks_with_completed_uploads.front().get();

    // It's now safe to release the pixel buffer and the shared memory.
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    bytes_pending_upload_ -= task->resource()->bytes();

    task->DidRun(false);

    DCHECK(std::find(completed_tasks_.begin(),
                     completed_tasks_.end(),
                     task) == completed_tasks_.end());
    completed_tasks_.push_back(task);

    tasks_required_for_activation_.erase(task);

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
  TRACE_EVENT0(
      "cc", "PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks");

  DCHECK(should_notify_client_if_no_tasks_are_pending_);

  check_for_completed_raster_tasks_callback_.Cancel();
  check_for_completed_raster_tasks_pending_ = false;

  RasterWorkerPool::CheckForCompletedTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Determine what client notifications to generate.
  bool will_notify_client_that_no_tasks_required_for_activation_are_pending =
      (should_notify_client_if_no_tasks_required_for_activation_are_pending_ &&
       !HasPendingTasksRequiredForActivation());
  bool will_notify_client_that_no_tasks_are_pending =
      (should_notify_client_if_no_tasks_are_pending_ &&
       !HasPendingTasks());

  // Adjust the need to generate notifications before scheduling more tasks.
  should_notify_client_if_no_tasks_required_for_activation_are_pending_ &=
      !will_notify_client_that_no_tasks_required_for_activation_are_pending;
  should_notify_client_if_no_tasks_are_pending_ &=
      !will_notify_client_that_no_tasks_are_pending;

  scheduled_raster_task_count_ = 0;
  if (PendingRasterTaskCount())
    ScheduleMoreTasks();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, StateName(),
      "state", TracedValue::FromValue(StateAsValue().release()));

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

  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();

    // |pixel_buffer_tasks_| contains all tasks that have not yet completed.
    TaskMap::iterator pixel_buffer_it = pixel_buffer_tasks_.find(task);
    if (pixel_buffer_it == pixel_buffer_tasks_.end())
      continue;

    // HasFinishedRunning() will return true when set pixels has completed.
    if (task->HasFinishedRunning()) {
      DCHECK(std::find(completed_tasks_.begin(),
                       completed_tasks_.end(),
                       task) != completed_tasks_.end());
      continue;
    }

    // All raster tasks need to be throttled by bytes of pending uploads.
    size_t new_bytes_pending_upload = bytes_pending_upload;
    new_bytes_pending_upload += task->resource()->bytes();
    if (new_bytes_pending_upload > max_bytes_pending_upload_)
      break;

    internal::WorkerPoolTask* pixel_buffer_task = pixel_buffer_it->second.get();

    // If raster has finished, just update |bytes_pending_upload|.
    if (pixel_buffer_task && pixel_buffer_task->HasCompleted()) {
      bytes_pending_upload = new_bytes_pending_upload;
      continue;
    }

    // Throttle raster tasks based on kMaxScheduledRasterTasks.
    size_t scheduled_raster_task_count =
        tasks[PREPAINT_TYPE].container().size() +
        tasks[REQUIRED_FOR_ACTIVATION_TYPE].container().size();
    if (scheduled_raster_task_count >= kMaxScheduledRasterTasks)
      break;

    // Update |bytes_pending_upload| now that task has cleared all
    // throttling limits.
    bytes_pending_upload = new_bytes_pending_upload;

    RasterTaskType type = IsRasterTaskRequiredForActivation(task) ?
        REQUIRED_FOR_ACTIVATION_TYPE :
        PREPAINT_TYPE;

    // Use existing pixel buffer task if available.
    if (pixel_buffer_task) {
      tasks[type].container().push_back(
          CreateGraphNodeForRasterTask(pixel_buffer_task,
                                       task->dependencies(),
                                       priority++,
                                       &graph));
      continue;
    }

    // Request a pixel buffer. This will reserve shared memory.
    resource_provider()->AcquirePixelBuffer(task->resource()->id());

    // MapPixelBuffer() returns NULL if context was lost at the time
    // AcquirePixelBuffer() was called. For simplicity we still post
    // a raster task that is essentially a noop in these situations.
    uint8* buffer = resource_provider()->MapPixelBuffer(
        task->resource()->id());

    scoped_refptr<internal::WorkerPoolTask> new_pixel_buffer_task(
        new PixelBufferWorkerPoolTaskImpl(
            task,
            buffer,
            base::Bind(&PixelBufferRasterWorkerPool::OnRasterTaskCompleted,
                       base::Unretained(this),
                       make_scoped_refptr(task))));
    pixel_buffer_tasks_[task] = new_pixel_buffer_task;
    tasks[type].container().push_back(
        CreateGraphNodeForRasterTask(new_pixel_buffer_task.get(),
                                     task->dependencies(),
                                     priority++,
                                     &graph));
  }

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task;

  size_t scheduled_raster_task_required_for_activation_count =
        tasks[REQUIRED_FOR_ACTIVATION_TYPE].container().size();
  DCHECK_LE(scheduled_raster_task_required_for_activation_count,
            tasks_required_for_activation_.size());
  // Schedule OnRasterTasksRequiredForActivationFinished call only when
  // notification is pending and throttling is not preventing all pending
  // tasks required for activation from being scheduled.
  if (scheduled_raster_task_required_for_activation_count ==
      tasks_required_for_activation_.size() &&
      should_notify_client_if_no_tasks_required_for_activation_are_pending_) {
    new_raster_required_for_activation_finished_task =
        CreateRasterRequiredForActivationFinishedTask();
    internal::GraphNode* raster_required_for_activation_finished_node =
        CreateGraphNodeForTask(
            new_raster_required_for_activation_finished_task.get(),
            0u,  // Priority 0
            &graph);
    AddDependenciesToGraphNode(
        raster_required_for_activation_finished_node,
        tasks[REQUIRED_FOR_ACTIVATION_TYPE].container());
  }

  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task;

  size_t scheduled_raster_task_count =
      tasks[PREPAINT_TYPE].container().size() +
      tasks[REQUIRED_FOR_ACTIVATION_TYPE].container().size();
  DCHECK_LE(scheduled_raster_task_count, PendingRasterTaskCount());
  // Schedule OnRasterTasksFinished call only when notification is pending
  // and throttling is not preventing all pending tasks from being scheduled.
  if (scheduled_raster_task_count == PendingRasterTaskCount() &&
      should_notify_client_if_no_tasks_are_pending_) {
    new_raster_finished_task = CreateRasterFinishedTask();
    internal::GraphNode* raster_finished_node =
        CreateGraphNodeForTask(new_raster_finished_task.get(),
                               1u,  // Priority 1
                               &graph);
    for (unsigned type = 0; type < NUM_TYPES; ++type) {
      AddDependenciesToGraphNode(
          raster_finished_node,
          tasks[type].container());
    }
  }

  SetTaskGraph(&graph);

  scheduled_raster_task_count_ = scheduled_raster_task_count;

  set_raster_finished_task(new_raster_finished_task);
  set_raster_required_for_activation_finished_task(
      new_raster_required_for_activation_finished_task);
}

void PixelBufferRasterWorkerPool::OnRasterTaskCompleted(
    scoped_refptr<internal::RasterWorkerPoolTask> task,
    bool was_canceled,
    bool needs_upload) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("cc"),
               "PixelBufferRasterWorkerPool::OnRasterTaskCompleted",
               "was_canceled", was_canceled,
               "needs_upload", needs_upload);

  DCHECK(pixel_buffer_tasks_.find(task.get()) != pixel_buffer_tasks_.end());

  // Balanced with MapPixelBuffer() call in ScheduleMoreTasks().
  resource_provider()->UnmapPixelBuffer(task->resource()->id());

  if (!needs_upload) {
    resource_provider()->ReleasePixelBuffer(task->resource()->id());

    if (was_canceled) {
      // When priorites change, a raster task can be canceled as a result of
      // no longer being of high enough priority to fit in our throttled
      // raster task budget. The task has not yet completed in this case.
      RasterTaskVector::const_iterator it = std::find(raster_tasks().begin(),
                                                      raster_tasks().end(),
                                                      task);
      if (it != raster_tasks().end()) {
        pixel_buffer_tasks_[task.get()] = NULL;
        return;
      }
    }

    task->DidRun(was_canceled);
    DCHECK(std::find(completed_tasks_.begin(),
                     completed_tasks_.end(),
                     task) == completed_tasks_.end());
    completed_tasks_.push_back(task);
    tasks_required_for_activation_.erase(task);
    return;
  }

  DCHECK(!was_canceled);

  resource_provider()->BeginSetPixels(task->resource()->id());
  has_performed_uploads_since_last_flush_ = true;

  bytes_pending_upload_ += task->resource()->bytes();
  tasks_with_pending_upload_.push_back(task);
}

unsigned PixelBufferRasterWorkerPool::PendingRasterTaskCount() const {
  unsigned num_completed_raster_tasks =
      tasks_with_pending_upload_.size() + completed_tasks_.size();
  DCHECK_GE(pixel_buffer_tasks_.size(), num_completed_raster_tasks);
  return pixel_buffer_tasks_.size() - num_completed_raster_tasks;
}

bool PixelBufferRasterWorkerPool::HasPendingTasks() const {
  return PendingRasterTaskCount() || !tasks_with_pending_upload_.empty();
}

bool PixelBufferRasterWorkerPool::HasPendingTasksRequiredForActivation() const {
  return !tasks_required_for_activation_.empty();
}

const char* PixelBufferRasterWorkerPool::StateName() const {
  if (scheduled_raster_task_count_)
    return "rasterizing";
  if (PendingRasterTaskCount())
    return "throttled";
  if (!tasks_with_pending_upload_.empty())
    return "waiting_for_uploads";

  return "finishing";
}

scoped_ptr<base::Value> PixelBufferRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetInteger("completed_count", completed_tasks_.size());
  state->SetInteger("pending_count", pixel_buffer_tasks_.size());
  state->SetInteger("pending_upload_count", tasks_with_pending_upload_.size());
  state->SetInteger("required_for_activation_count",
                    tasks_required_for_activation_.size());
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
