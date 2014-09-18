// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_copy_raster_worker_pool.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/debug/trace_event_argument.h"
#include "base/strings/stringprintf.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/scoped_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace cc {

// static
scoped_ptr<RasterWorkerPool> ImageCopyRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool) {
  return make_scoped_ptr<RasterWorkerPool>(
      new ImageCopyRasterWorkerPool(task_runner,
                                    task_graph_runner,
                                    context_provider,
                                    resource_provider,
                                    resource_pool));
}

ImageCopyRasterWorkerPool::ImageCopyRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    ResourcePool* resource_pool)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      context_provider_(context_provider),
      resource_provider_(resource_provider),
      resource_pool_(resource_pool),
      has_performed_copy_since_last_flush_(false),
      raster_finished_weak_ptr_factory_(this) {
  DCHECK(context_provider_);
}

ImageCopyRasterWorkerPool::~ImageCopyRasterWorkerPool() {
  DCHECK_EQ(0u, raster_task_states_.size());
}

Rasterizer* ImageCopyRasterWorkerPool::AsRasterizer() { return this; }

void ImageCopyRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void ImageCopyRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void ImageCopyRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::ScheduleTasks");

  if (raster_pending_.none())
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  // Mark all task sets as pending.
  raster_pending_.set();

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask> new_raster_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_raster_finished_tasks[task_set] = CreateRasterFinishedTask(
        task_runner_.get(),
        base::Bind(&ImageCopyRasterWorkerPool::OnRasterFinished,
                   raster_finished_weak_ptr_factory_.GetWeakPtr(),
                   task_set));
  }

  resource_pool_->CheckBusyResources();

  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (!item.task_sets[task_set])
        continue;

      ++task_count[task_set];

      graph_.edges.push_back(
          TaskGraph::Edge(task, new_raster_finished_tasks[task_set].get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);
  }

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    InsertNodeForTask(&graph_,
                      new_raster_finished_tasks[task_set].get(),
                      kRasterFinishedTaskPriority,
                      task_count[task_set]);
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  std::copy(new_raster_finished_tasks,
            new_raster_finished_tasks + kNumberOfTaskSets,
            raster_finished_tasks_);

  resource_pool_->ReduceResourceUsage();

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, "rasterizing", "state", StateAsValue());
}

void ImageCopyRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "ImageCopyRasterWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    RasterizerTask* task = static_cast<RasterizerTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();

  FlushCopies();
}

RasterBuffer* ImageCopyRasterWorkerPool::AcquireBufferForRaster(
    RasterTask* task) {
  DCHECK_EQ(task->resource()->format(), resource_pool_->resource_format());
  scoped_ptr<ScopedResource> resource(
      resource_pool_->AcquireResource(task->resource()->size()));
  RasterBuffer* raster_buffer =
      resource_provider_->AcquireImageRasterBuffer(resource->id());
  DCHECK(std::find_if(raster_task_states_.begin(),
                      raster_task_states_.end(),
                      RasterTaskState::TaskComparator(task)) ==
         raster_task_states_.end());
  raster_task_states_.push_back(RasterTaskState(task, resource.release()));
  return raster_buffer;
}

void ImageCopyRasterWorkerPool::ReleaseBufferForRaster(RasterTask* task) {
  RasterTaskState::Vector::iterator it =
      std::find_if(raster_task_states_.begin(),
                   raster_task_states_.end(),
                   RasterTaskState::TaskComparator(task));
  DCHECK(it != raster_task_states_.end());
  scoped_ptr<ScopedResource> resource(it->resource);
  std::swap(*it, raster_task_states_.back());
  raster_task_states_.pop_back();

  bool content_has_changed =
      resource_provider_->ReleaseImageRasterBuffer(resource->id());

  // |content_has_changed| can be false as result of task being canceled or
  // task implementation deciding not to modify bitmap (ie. analysis of raster
  // commands detected content as a solid color).
  if (content_has_changed) {
    resource_provider_->CopyResource(resource->id(), task->resource()->id());
    has_performed_copy_since_last_flush_ = true;
  }

  resource_pool_->ReleaseResource(resource.Pass());
}

void ImageCopyRasterWorkerPool::OnRasterFinished(TaskSet task_set) {
  TRACE_EVENT1("cc",
               "ImageCopyRasterWorkerPool::OnRasterFinished",
               "task_set",
               task_set);

  DCHECK(raster_pending_[task_set]);
  raster_pending_[task_set] = false;
  if (raster_pending_.any()) {
    TRACE_EVENT_ASYNC_STEP_INTO1(
        "cc", "ScheduledTasks", this, "rasterizing", "state", StateAsValue());
  } else {
    TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  }
  client_->DidFinishRunningTasks(task_set);
}

void ImageCopyRasterWorkerPool::FlushCopies() {
  if (!has_performed_copy_since_last_flush_)
    return;

  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  has_performed_copy_since_last_flush_ = false;
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
ImageCopyRasterWorkerPool::StateAsValue() const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();

  state->BeginArray("tasks_pending");
  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set)
    state->AppendBoolean(raster_pending_[task_set]);
  state->EndArray();
  state->BeginDictionary("staging_state");
  StagingStateAsValueInto(state.get());
  state->EndDictionary();

  return state;
}
void ImageCopyRasterWorkerPool::StagingStateAsValueInto(
    base::debug::TracedValue* staging_state) const {
  staging_state->SetInteger("staging_resource_count",
                            resource_pool_->total_resource_count());
  staging_state->SetInteger("bytes_used_for_staging_resources",
                            resource_pool_->total_memory_usage_bytes());
  staging_state->SetInteger("pending_copy_count",
                            resource_pool_->total_resource_count() -
                                resource_pool_->acquired_resource_count());
  staging_state->SetInteger("bytes_pending_copy",
                            resource_pool_->total_memory_usage_bytes() -
                                resource_pool_->acquired_memory_usage_bytes());
}

}  // namespace cc
