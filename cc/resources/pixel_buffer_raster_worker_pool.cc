// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/pixel_buffer_raster_worker_pool.h"

#include <algorithm>

#include "base/containers/stack_container.h"
#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/strings/stringprintf.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl(ResourceProvider* resource_provider,
                   const Resource* resource)
      : resource_provider_(resource_provider),
        resource_(resource),
        buffer_(NULL),
        stride_(0) {
    resource_provider_->AcquirePixelBuffer(resource_->id());
    buffer_ = resource_provider_->MapPixelBuffer(resource_->id(), &stride_);
  }

  virtual ~RasterBufferImpl() {
    resource_provider_->ReleasePixelBuffer(resource_->id());
  }

  // Overridden from RasterBuffer:
  virtual skia::RefPtr<SkCanvas> AcquireSkCanvas() OVERRIDE {
    if (!buffer_)
      return skia::AdoptRef(SkCreateNullCanvas());

    RasterWorkerPool::AcquireBitmapForBuffer(
        &bitmap_, buffer_, resource_->format(), resource_->size(), stride_);
    return skia::AdoptRef(new SkCanvas(bitmap_));
  }
  virtual void ReleaseSkCanvas(const skia::RefPtr<SkCanvas>& canvas) OVERRIDE {
    if (!buffer_)
      return;

    RasterWorkerPool::ReleaseBitmapForBuffer(
        &bitmap_, buffer_, resource_->format());
  }

 private:
  ResourceProvider* resource_provider_;
  const Resource* resource_;
  uint8_t* buffer_;
  int stride_;
  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

const int kCheckForCompletedRasterTasksDelayMs = 6;

const size_t kMaxScheduledRasterTasks = 48;

typedef base::StackVector<RasterTask*, kMaxScheduledRasterTasks>
    RasterTaskVector;

TaskSetCollection NonEmptyTaskSetsFromTaskCounts(const size_t* task_counts) {
  TaskSetCollection task_sets;
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    if (task_counts[task_set])
      task_sets[task_set] = true;
  }
  return task_sets;
}

void AddTaskSetsToTaskCounts(size_t* task_counts,
                             const TaskSetCollection& task_sets) {
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    if (task_sets[task_set])
      task_counts[task_set]++;
  }
}

void RemoveTaskSetsFromTaskCounts(size_t* task_counts,
                                  const TaskSetCollection& task_sets) {
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    if (task_sets[task_set])
      task_counts[task_set]--;
  }
}

}  // namespace

PixelBufferRasterWorkerPool::RasterTaskState::RasterTaskState(
    RasterTask* task,
    const TaskSetCollection& task_sets)
    : type(UNSCHEDULED), task(task), task_sets(task_sets) {
}

// static
scoped_ptr<RasterWorkerPool> PixelBufferRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    size_t max_transfer_buffer_usage_bytes) {
  return make_scoped_ptr<RasterWorkerPool>(
      new PixelBufferRasterWorkerPool(task_runner,
                                      task_graph_runner,
                                      context_provider,
                                      resource_provider,
                                      max_transfer_buffer_usage_bytes));
}

PixelBufferRasterWorkerPool::PixelBufferRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    size_t max_transfer_buffer_usage_bytes)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      context_provider_(context_provider),
      resource_provider_(resource_provider),
      shutdown_(false),
      scheduled_raster_task_count_(0u),
      bytes_pending_upload_(0u),
      max_bytes_pending_upload_(max_transfer_buffer_usage_bytes),
      has_performed_uploads_since_last_flush_(false),
      check_for_completed_raster_task_notifier_(
          task_runner,
          base::Bind(&PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(
              kCheckForCompletedRasterTasksDelayMs)),
      raster_finished_weak_ptr_factory_(this) {
  DCHECK(context_provider_);
  std::fill(task_counts_, task_counts_ + kNumberOfTaskSets, 0);
}

PixelBufferRasterWorkerPool::~PixelBufferRasterWorkerPool() {
  DCHECK_EQ(0u, raster_task_states_.size());
  DCHECK_EQ(0u, raster_tasks_with_pending_upload_.size());
  DCHECK_EQ(0u, completed_raster_tasks_.size());
  DCHECK_EQ(0u, completed_image_decode_tasks_.size());
  DCHECK(NonEmptyTaskSetsFromTaskCounts(task_counts_).none());
}

Rasterizer* PixelBufferRasterWorkerPool::AsRasterizer() { return this; }

void PixelBufferRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void PixelBufferRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::Shutdown");

  shutdown_ = true;

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);

  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();

  check_for_completed_raster_task_notifier_.Cancel();

  for (RasterTaskState::Vector::iterator it = raster_task_states_.begin();
       it != raster_task_states_.end();
       ++it) {
    RasterTaskState& state = *it;

    // All unscheduled tasks need to be canceled.
    if (state.type == RasterTaskState::UNSCHEDULED) {
      completed_raster_tasks_.push_back(state.task);
      state.type = RasterTaskState::COMPLETED;
    }
  }
  DCHECK_EQ(completed_raster_tasks_.size(), raster_task_states_.size());
}

void PixelBufferRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleTasks");

  if (should_notify_client_if_no_tasks_are_pending_.none())
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  should_notify_client_if_no_tasks_are_pending_.set();
  std::fill(task_counts_, task_counts_ + kNumberOfTaskSets, 0);

  // Update raster task state and remove items from old queue.
  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;

    // Remove any old items that are associated with this task. The result is
    // that the old queue is left with all items not present in this queue,
    // which we use below to determine what tasks need to be canceled.
    RasterTaskQueue::Item::Vector::iterator old_it =
        std::find_if(raster_tasks_.items.begin(),
                     raster_tasks_.items.end(),
                     RasterTaskQueue::Item::TaskComparator(task));
    if (old_it != raster_tasks_.items.end()) {
      std::swap(*old_it, raster_tasks_.items.back());
      raster_tasks_.items.pop_back();
    }

    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    if (state_it != raster_task_states_.end()) {
      RasterTaskState& state = *state_it;

      state.task_sets = item.task_sets;
      // |raster_tasks_required_for_activation_count| accounts for all tasks
      // that need to complete before we can send a "ready to activate" signal.
      // Tasks that have already completed should not be part of this count.
      if (state.type != RasterTaskState::COMPLETED)
        AddTaskSetsToTaskCounts(task_counts_, item.task_sets);

      continue;
    }

    DCHECK(!task->HasBeenScheduled());
    raster_task_states_.push_back(RasterTaskState(task, item.task_sets));
    AddTaskSetsToTaskCounts(task_counts_, item.task_sets);
  }

  // Determine what tasks in old queue need to be canceled.
  for (RasterTaskQueue::Item::Vector::const_iterator it =
           raster_tasks_.items.begin();
       it != raster_tasks_.items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;

    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    // We've already processed completion if we can't find a RasterTaskState for
    // this task.
    if (state_it == raster_task_states_.end())
      continue;

    RasterTaskState& state = *state_it;

    // Unscheduled task can be canceled.
    if (state.type == RasterTaskState::UNSCHEDULED) {
      DCHECK(!task->HasBeenScheduled());
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(task);
      state.type = RasterTaskState::COMPLETED;
    }

    // No longer in any task set.
    state.task_sets.reset();
  }

  raster_tasks_.Swap(queue);

  // Check for completed tasks when ScheduleTasks() is called as
  // priorities might have changed and this maximizes the number
  // of top priority tasks that are scheduled.
  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Schedule new tasks.
  ScheduleMoreTasks();

  // Reschedule check for completed raster tasks.
  check_for_completed_raster_task_notifier_.Schedule();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, StateName(), "state", StateAsValue());
}

void PixelBufferRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::CheckForCompletedTasks");

  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();
  FlushUploads();

  for (RasterizerTask::Vector::const_iterator it =
           completed_image_decode_tasks_.begin();
       it != completed_image_decode_tasks_.end();
       ++it) {
    RasterizerTask* task = it->get();
    task->RunReplyOnOriginThread();
  }
  completed_image_decode_tasks_.clear();

  for (RasterTask::Vector::const_iterator it = completed_raster_tasks_.begin();
       it != completed_raster_tasks_.end();
       ++it) {
    RasterTask* task = it->get();
    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    DCHECK(state_it != raster_task_states_.end());
    DCHECK_EQ(RasterTaskState::COMPLETED, state_it->type);

    std::swap(*state_it, raster_task_states_.back());
    raster_task_states_.pop_back();

    task->RunReplyOnOriginThread();
  }
  completed_raster_tasks_.clear();
}

scoped_ptr<RasterBuffer> PixelBufferRasterWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  return make_scoped_ptr<RasterBuffer>(
      new RasterBufferImpl(resource_provider_, resource));
}

void PixelBufferRasterWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void PixelBufferRasterWorkerPool::OnRasterFinished(TaskSet task_set) {
  TRACE_EVENT2("cc",
               "PixelBufferRasterWorkerPool::OnRasterFinished",
               "task_set",
               task_set,
               "should_notify_client_if_no_tasks_are_pending",
               should_notify_client_if_no_tasks_are_pending_[task_set]);

  // There's no need to call CheckForCompletedRasterTasks() if the client has
  // already been notified.
  if (!should_notify_client_if_no_tasks_are_pending_[task_set])
    return;
  raster_finished_tasks_pending_[task_set] = false;

  // This reduces latency between the time when all tasks required for
  // activation have finished running and the time when the client is
  // notified.
  CheckForCompletedRasterTasks();
}

void PixelBufferRasterWorkerPool::FlushUploads() {
  if (!has_performed_uploads_since_last_flush_)
    return;

  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  has_performed_uploads_since_last_flush_ = false;
}

void PixelBufferRasterWorkerPool::CheckForCompletedUploads() {
  RasterTask::Vector tasks_with_completed_uploads;

  // First check if any have completed.
  while (!raster_tasks_with_pending_upload_.empty()) {
    RasterTask* task = raster_tasks_with_pending_upload_.front().get();
    DCHECK(std::find_if(raster_task_states_.begin(),
                        raster_task_states_.end(),
                        RasterTaskState::TaskComparator(task)) !=
           raster_task_states_.end());
    DCHECK_EQ(RasterTaskState::UPLOADING,
              std::find_if(raster_task_states_.begin(),
                           raster_task_states_.end(),
                           RasterTaskState::TaskComparator(task))->type);

    // Uploads complete in the order they are issued.
    if (!resource_provider_->DidSetPixelsComplete(task->resource()->id()))
      break;

    tasks_with_completed_uploads.push_back(task);
    raster_tasks_with_pending_upload_.pop_front();
  }

  DCHECK(client_);
  TaskSetCollection tasks_that_should_be_forced_to_complete =
      client_->TasksThatShouldBeForcedToComplete();
  bool should_force_some_uploads_to_complete =
      shutdown_ || tasks_that_should_be_forced_to_complete.any();

  if (should_force_some_uploads_to_complete) {
    RasterTask::Vector tasks_with_uploads_to_force;
    RasterTaskDeque::iterator it = raster_tasks_with_pending_upload_.begin();
    while (it != raster_tasks_with_pending_upload_.end()) {
      RasterTask* task = it->get();
      RasterTaskState::Vector::const_iterator state_it =
          std::find_if(raster_task_states_.begin(),
                       raster_task_states_.end(),
                       RasterTaskState::TaskComparator(task));
      DCHECK(state_it != raster_task_states_.end());
      const RasterTaskState& state = *state_it;

      // Force all uploads to complete for which the client requests to do so.
      // During shutdown, force all pending uploads to complete.
      if (shutdown_ ||
          (state.task_sets & tasks_that_should_be_forced_to_complete).any()) {
        tasks_with_uploads_to_force.push_back(task);
        tasks_with_completed_uploads.push_back(task);
        it = raster_tasks_with_pending_upload_.erase(it);
        continue;
      }

      ++it;
    }

    // Force uploads in reverse order. Since forcing can cause a wait on
    // all previous uploads, we would rather wait only once downstream.
    for (RasterTask::Vector::reverse_iterator it =
             tasks_with_uploads_to_force.rbegin();
         it != tasks_with_uploads_to_force.rend();
         ++it) {
      RasterTask* task = it->get();

      resource_provider_->ForceSetPixelsToComplete(task->resource()->id());
      has_performed_uploads_since_last_flush_ = true;
    }
  }

  // Release shared memory and move tasks with completed uploads
  // to |completed_raster_tasks_|.
  for (RasterTask::Vector::const_iterator it =
           tasks_with_completed_uploads.begin();
       it != tasks_with_completed_uploads.end();
       ++it) {
    RasterTask* task = it->get();
    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    DCHECK(state_it != raster_task_states_.end());
    RasterTaskState& state = *state_it;

    bytes_pending_upload_ -= task->resource()->bytes();

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    // Async set pixels commands are not necessarily processed in-sequence with
    // drawing commands. Read lock fences are required to ensure that async
    // commands don't access the resource while used for drawing.
    resource_provider_->EnableReadLockFences(task->resource()->id());

    DCHECK(std::find(completed_raster_tasks_.begin(),
                     completed_raster_tasks_.end(),
                     task) == completed_raster_tasks_.end());
    completed_raster_tasks_.push_back(task);
    state.type = RasterTaskState::COMPLETED;
    // Triggers if the current task belongs to a set that should be empty.
    DCHECK((state.task_sets & ~NonEmptyTaskSetsFromTaskCounts(task_counts_))
               .none());
    RemoveTaskSetsFromTaskCounts(task_counts_, state.task_sets);
  }
}

void PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks() {
  TRACE_EVENT0("cc",
               "PixelBufferRasterWorkerPool::CheckForCompletedRasterTasks");

  // Since this function can be called directly, cancel any pending checks.
  check_for_completed_raster_task_notifier_.Cancel();

  DCHECK(should_notify_client_if_no_tasks_are_pending_.any());

  CheckForCompletedRasterizerTasks();
  CheckForCompletedUploads();
  FlushUploads();

  // Determine what client notifications to generate.
  TaskSetCollection will_notify_client_that_no_tasks_are_pending =
      should_notify_client_if_no_tasks_are_pending_ &
      ~raster_finished_tasks_pending_ & ~PendingTasks();

  // Adjust the need to generate notifications before scheduling more tasks.
  should_notify_client_if_no_tasks_are_pending_ &=
      ~will_notify_client_that_no_tasks_are_pending;

  scheduled_raster_task_count_ = 0;
  if (PendingRasterTaskCount())
    ScheduleMoreTasks();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, StateName(), "state", StateAsValue());

  // Schedule another check for completed raster tasks while there are
  // pending raster tasks or pending uploads.
  if (PendingTasks().any())
    check_for_completed_raster_task_notifier_.Schedule();

  if (should_notify_client_if_no_tasks_are_pending_.none())
    TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);

  // Generate client notifications.
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    if (will_notify_client_that_no_tasks_are_pending[task_set]) {
      DCHECK(!PendingTasks()[task_set]);
      client_->DidFinishRunningTasks(task_set);
    }
  }
}

void PixelBufferRasterWorkerPool::ScheduleMoreTasks() {
  TRACE_EVENT0("cc", "PixelBufferRasterWorkerPool::ScheduleMoreTasks");

  RasterTaskVector tasks[kNumberOfTaskSets];

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  size_t bytes_pending_upload = bytes_pending_upload_;
  TaskSetCollection did_throttle_raster_tasks;
  size_t scheduled_raster_task_count = 0;

  for (RasterTaskQueue::Item::Vector::const_iterator it =
           raster_tasks_.items.begin();
       it != raster_tasks_.items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(item.task_sets.any());

    // |raster_task_states_| contains the state of all tasks that we have not
    // yet run reply callbacks for.
    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(task));
    if (state_it == raster_task_states_.end())
      continue;

    RasterTaskState& state = *state_it;

    // Skip task if completed.
    if (state.type == RasterTaskState::COMPLETED) {
      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       task) != completed_raster_tasks_.end());
      continue;
    }

    // All raster tasks need to be throttled by bytes of pending uploads,
    // but if it's the only task allow it to complete no matter what its size,
    // to prevent starvation of the task queue.
    size_t new_bytes_pending_upload = bytes_pending_upload;
    new_bytes_pending_upload += task->resource()->bytes();
    if (new_bytes_pending_upload > max_bytes_pending_upload_ &&
        bytes_pending_upload) {
      did_throttle_raster_tasks |= item.task_sets;
      continue;
    }

    // If raster has finished, just update |bytes_pending_upload|.
    if (state.type == RasterTaskState::UPLOADING) {
      DCHECK(!task->HasCompleted());
      bytes_pending_upload = new_bytes_pending_upload;
      continue;
    }

    // Throttle raster tasks based on kMaxScheduledRasterTasks.
    if (scheduled_raster_task_count >= kMaxScheduledRasterTasks) {
      did_throttle_raster_tasks |= item.task_sets;
      continue;
    }

    // Update |bytes_pending_upload| now that task has cleared all
    // throttling limits.
    bytes_pending_upload = new_bytes_pending_upload;

    DCHECK(state.type == RasterTaskState::UNSCHEDULED ||
           state.type == RasterTaskState::SCHEDULED);
    state.type = RasterTaskState::SCHEDULED;

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);

    ++scheduled_raster_task_count;
    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (item.task_sets[task_set])
        tasks[task_set].container().push_back(task);
    }
  }

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask> new_raster_finished_tasks[kNumberOfTaskSets];
  size_t scheduled_task_counts[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    scheduled_task_counts[task_set] = tasks[task_set].container().size();
    DCHECK_LE(scheduled_task_counts[task_set], task_counts_[task_set]);
    // Schedule OnRasterFinished call for task set only when notification is
    // pending and throttling is not preventing all pending tasks in the set
    // from being scheduled.
    if (!did_throttle_raster_tasks[task_set] &&
        should_notify_client_if_no_tasks_are_pending_[task_set]) {
      new_raster_finished_tasks[task_set] = CreateRasterFinishedTask(
          task_runner_.get(),
          base::Bind(&PixelBufferRasterWorkerPool::OnRasterFinished,
                     raster_finished_weak_ptr_factory_.GetWeakPtr(),
                     task_set));
      raster_finished_tasks_pending_[task_set] = true;
      InsertNodeForTask(&graph_,
                        new_raster_finished_tasks[task_set].get(),
                        kRasterFinishedTaskPriority,
                        scheduled_task_counts[task_set]);
      for (RasterTaskVector::ContainerType::const_iterator it =
               tasks[task_set].container().begin();
           it != tasks[task_set].container().end();
           ++it) {
        graph_.edges.push_back(
            TaskGraph::Edge(*it, new_raster_finished_tasks[task_set].get()));
      }
    }
  }

  DCHECK_LE(scheduled_raster_task_count, PendingRasterTaskCount());

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  scheduled_raster_task_count_ = scheduled_raster_task_count;

  std::copy(new_raster_finished_tasks,
            new_raster_finished_tasks + kNumberOfTaskSets,
            raster_finished_tasks_);
}

unsigned PixelBufferRasterWorkerPool::PendingRasterTaskCount() const {
  unsigned num_completed_raster_tasks =
      raster_tasks_with_pending_upload_.size() + completed_raster_tasks_.size();
  DCHECK_GE(raster_task_states_.size(), num_completed_raster_tasks);
  return raster_task_states_.size() - num_completed_raster_tasks;
}

TaskSetCollection PixelBufferRasterWorkerPool::PendingTasks() const {
  return NonEmptyTaskSetsFromTaskCounts(task_counts_);
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

void PixelBufferRasterWorkerPool::CheckForCompletedRasterizerTasks() {
  TRACE_EVENT0("cc",
               "PixelBufferRasterWorkerPool::CheckForCompletedRasterizerTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    RasterizerTask* task = static_cast<RasterizerTask*>(it->get());

    RasterTask* raster_task = task->AsRasterTask();
    if (!raster_task) {
      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      completed_image_decode_tasks_.push_back(task);
      continue;
    }

    RasterTaskState::Vector::iterator state_it =
        std::find_if(raster_task_states_.begin(),
                     raster_task_states_.end(),
                     RasterTaskState::TaskComparator(raster_task));
    DCHECK(state_it != raster_task_states_.end());

    RasterTaskState& state = *state_it;
    DCHECK_EQ(RasterTaskState::SCHEDULED, state.type);

    resource_provider_->UnmapPixelBuffer(raster_task->resource()->id());

    if (!raster_task->HasFinishedRunning()) {
      // When priorites change, a raster task can be canceled as a result of
      // no longer being of high enough priority to fit in our throttled
      // raster task budget. The task has not yet completed in this case.
      raster_task->WillComplete();
      raster_task->CompleteOnOriginThread(this);
      raster_task->DidComplete();

      RasterTaskQueue::Item::Vector::const_iterator item_it =
          std::find_if(raster_tasks_.items.begin(),
                       raster_tasks_.items.end(),
                       RasterTaskQueue::Item::TaskComparator(raster_task));
      if (item_it != raster_tasks_.items.end()) {
        state.type = RasterTaskState::UNSCHEDULED;
        continue;
      }

      DCHECK(std::find(completed_raster_tasks_.begin(),
                       completed_raster_tasks_.end(),
                       raster_task) == completed_raster_tasks_.end());
      completed_raster_tasks_.push_back(raster_task);
      state.type = RasterTaskState::COMPLETED;
      // Triggers if the current task belongs to a set that should be empty.
      DCHECK((state.task_sets & ~NonEmptyTaskSetsFromTaskCounts(task_counts_))
                 .none());
      RemoveTaskSetsFromTaskCounts(task_counts_, state.task_sets);
      continue;
    }

    resource_provider_->BeginSetPixels(raster_task->resource()->id());
    has_performed_uploads_since_last_flush_ = true;

    bytes_pending_upload_ += raster_task->resource()->bytes();
    raster_tasks_with_pending_upload_.push_back(raster_task);
    state.type = RasterTaskState::UPLOADING;
  }
  completed_tasks_.clear();
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
PixelBufferRasterWorkerPool::StateAsValue() const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  state->SetInteger("completed_count", completed_raster_tasks_.size());
  state->BeginArray("pending_count");
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set)
    state->AppendInteger(task_counts_[task_set]);
  state->EndArray();
  state->SetInteger("pending_upload_count",
                    raster_tasks_with_pending_upload_.size());
  state->BeginDictionary("throttle_state");
  ThrottleStateAsValueInto(state.get());
  state->EndDictionary();
  return state;
}

void PixelBufferRasterWorkerPool::ThrottleStateAsValueInto(
    base::debug::TracedValue* throttle_state) const {
  throttle_state->SetInteger("bytes_available_for_upload",
                             max_bytes_pending_upload_ - bytes_pending_upload_);
  throttle_state->SetInteger("bytes_pending_upload", bytes_pending_upload_);
  throttle_state->SetInteger("scheduled_raster_task_count",
                             scheduled_raster_task_count_);
}

}  // namespace cc
