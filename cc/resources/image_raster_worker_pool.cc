// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource.h"

namespace cc {

// static
scoped_ptr<ImageRasterWorkerPool> ImageRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    ResourceProvider* resource_provider,
    unsigned texture_target) {
  return make_scoped_ptr(new ImageRasterWorkerPool(
      task_runner, GetTaskGraphRunner(), resource_provider, texture_target));
}

ImageRasterWorkerPool::ImageRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    internal::TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider,
    unsigned texture_target)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()),
      resource_provider_(resource_provider),
      texture_target_(texture_target),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false),
      raster_finished_weak_ptr_factory_(this) {}

ImageRasterWorkerPool::~ImageRasterWorkerPool() {}

void ImageRasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
}

void ImageRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::Shutdown");

  internal::TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

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

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              queue->required_for_activation_count,
              task_runner_.get(),
              base::Bind(
                  &ImageRasterWorkerPool::OnRasterRequiredForActivationFinished,
                  raster_finished_weak_ptr_factory_.GetWeakPtr())));
  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task(
      CreateRasterFinishedTask(
          task_runner_.get(),
          base::Bind(&ImageRasterWorkerPool::OnRasterFinished,
                     raster_finished_weak_ptr_factory_.GetWeakPtr())));

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

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);

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

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  raster_finished_task_ = new_raster_finished_task;
  raster_required_for_activation_finished_task_ =
      new_raster_required_for_activation_finished_task;

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
  return resource_provider_->best_texture_format();
}

void ImageRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
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
  return resource_provider_->MapImageRasterBuffer(resource->id());
}

void ImageRasterWorkerPool::ReleaseCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  resource_provider_->UnmapImageRasterBuffer(resource->id());
}

void ImageRasterWorkerPool::OnRasterFinished() {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::OnRasterFinished");

  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  TRACE_EVENT_ASYNC_END0("cc", "ScheduledTasks", this);
  client_->DidFinishRunningTasks();
}

void ImageRasterWorkerPool::OnRasterRequiredForActivationFinished() {
  TRACE_EVENT0("cc",
               "ImageRasterWorkerPool::OnRasterRequiredForActivationFinished");

  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc",
      "ScheduledTasks",
      this,
      "rasterizing",
      "state",
      TracedValue::FromValue(StateAsValue().release()));
  client_->DidFinishRunningTasksRequiredForActivation();
}

scoped_ptr<base::Value> ImageRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetBoolean("tasks_required_for_activation_pending",
                    raster_tasks_required_for_activation_pending_);
  return state.PassAs<base::Value>();
}

}  // namespace cc
