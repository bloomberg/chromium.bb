// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource.h"

namespace cc {

// static
scoped_ptr<RasterWorkerPool> ImageRasterWorkerPool::Create(
    ResourceProvider* resource_provider,
    unsigned texture_target) {
  return make_scoped_ptr<RasterWorkerPool>(
      new ImageRasterWorkerPool(GetTaskGraphRunner(),
                                resource_provider,
                                texture_target));
}

ImageRasterWorkerPool::ImageRasterWorkerPool(
    internal::TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    unsigned texture_target)
    : RasterWorkerPool(task_graph_runner, resource_provider),
      texture_target_(texture_target),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false) {}

ImageRasterWorkerPool::~ImageRasterWorkerPool() {}

void ImageRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

  if (!raster_tasks_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  raster_tasks_pending_ = true;
  raster_tasks_required_for_activation_pending_ = true;

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              queue->required_for_activation_count));
  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task(
      CreateRasterFinishedTask());

  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    internal::RasterWorkerPoolTask* task = item.task;
    DCHECK(!task->HasCompleted());

    if (item.required_for_activation) {
      graph_.edges.push_back(internal::TaskGraph::Edge(
          task, new_raster_required_for_activation_finished_task.get()));
    }

    InsertNodeForRasterTask(&graph_, task, task->dependencies(), priority++);

    graph_.edges.push_back(
        internal::TaskGraph::Edge(task, new_raster_finished_task.get()));
  }

  InsertNodeForTask(&graph_,
                    new_raster_required_for_activation_finished_task.get(),
                    kRasterRequiredForActivationFinishedTaskPriority,
                    queue->required_for_activation_count);
  InsertNodeForTask(&graph_,
                    new_raster_finished_task.get(),
                    kRasterFinishedTaskPriority,
                    queue->items.size());

  SetTaskGraph(&graph_);

  set_raster_finished_task(new_raster_finished_task);
  set_raster_required_for_activation_finished_task(
      new_raster_required_for_activation_finished_task);

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      "rasterizing",
      "state",
      TracedValue::FromValue(StateAsValue().release()));
}

unsigned ImageRasterWorkerPool::GetResourceTarget() const {
  return texture_target_;
}

ResourceFormat ImageRasterWorkerPool::GetResourceFormat() const {
  return resource_provider()->best_texture_format();
}

void ImageRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::CheckForCompletedTasks");

  CollectCompletedWorkerPoolTasks(&completed_tasks_);
  for (internal::Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    internal::WorkerPoolTask* task =
        static_cast<internal::WorkerPoolTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(this);
    task->DidComplete();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

SkCanvas* ImageRasterWorkerPool::AcquireCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  return resource_provider()->MapImageRasterBuffer(resource->id());
}

void ImageRasterWorkerPool::ReleaseCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  resource_provider()->UnmapImageRasterBuffer(resource->id());
}

void ImageRasterWorkerPool::OnRasterTasksFinished() {
  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  client()->DidFinishRunningTasks();
}

void ImageRasterWorkerPool::OnRasterTasksRequiredForActivationFinished() {
  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      "rasterizing",
      "state",
      TracedValue::FromValue(StateAsValue().release()));
  client()->DidFinishRunningTasksRequiredForActivation();
}

scoped_ptr<base::Value> ImageRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetBoolean("tasks_required_for_activation_pending",
                    raster_tasks_required_for_activation_pending_);
  return state.PassAs<base::Value>();
}

}  // namespace cc
