// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/image_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "base/values.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/resource.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"

namespace cc {

ImageRasterWorkerPool::ImageRasterWorkerPool(
    ResourceProvider* resource_provider,
    ContextProvider* context_provider,
    unsigned texture_target)
    : RasterWorkerPool(resource_provider, context_provider),
      texture_target_(texture_target),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false) {}

ImageRasterWorkerPool::~ImageRasterWorkerPool() {}

void ImageRasterWorkerPool::ScheduleTasks(RasterTask::Queue* queue) {
  TRACE_EVENT0("cc", "ImageRasterWorkerPool::ScheduleTasks");

  RasterWorkerPool::SetRasterTasks(queue);

  if (!raster_tasks_pending_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ScheduledTasks", this);

  raster_tasks_pending_ = true;
  raster_tasks_required_for_activation_pending_ = true;

  unsigned priority = 0u;
  TaskGraph graph;

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              raster_tasks_required_for_activation().size()));
  internal::GraphNode* raster_required_for_activation_finished_node =
      CreateGraphNodeForTask(
          new_raster_required_for_activation_finished_task.get(),
          priority++,
          &graph);

  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task(
      CreateRasterFinishedTask());
  internal::GraphNode* raster_finished_node = CreateGraphNodeForTask(
      new_raster_finished_task.get(), priority++, &graph);

  RasterTaskVector gpu_raster_tasks;

  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end();
       ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(!task->HasCompleted());

    if (task->use_gpu_rasterization()) {
      gpu_raster_tasks.push_back(task);
      continue;
    }

    task->ScheduleOnOriginThread(this);

    CreateGraphNodeForImageRasterTask(
        task,
        task->dependencies(),
        priority++,
        IsRasterTaskRequiredForActivation(task),
        raster_required_for_activation_finished_node,
        raster_finished_node,
        &graph);
  }

  SetTaskGraph(&graph);

  set_raster_finished_task(new_raster_finished_task);
  set_raster_required_for_activation_finished_task(
      new_raster_required_for_activation_finished_task);

  RunGpuRasterTasks(gpu_raster_tasks);

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

    task->RunReplyOnOriginThread();
  }

  CheckForCompletedGpuRasterTasks();
}

void* ImageRasterWorkerPool::AcquireBufferForRaster(
    internal::RasterWorkerPoolTask* task,
    int* stride) {
  // Acquire image for resource.
  resource_provider()->AcquireImage(task->resource()->id());

  *stride = resource_provider()->GetImageStride(task->resource()->id());
  return resource_provider()->MapImage(task->resource()->id());
}

void ImageRasterWorkerPool::OnRasterCompleted(
    internal::RasterWorkerPoolTask* task,
    const PicturePileImpl::Analysis& analysis) {
  resource_provider()->UnmapImage(task->resource()->id());
}

void ImageRasterWorkerPool::OnImageDecodeCompleted(
    internal::WorkerPoolTask* task) {}

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
  state->Set("scheduled_state", ScheduledStateAsValue().release());
  return state.PassAs<base::Value>();
}

// static
void ImageRasterWorkerPool::CreateGraphNodeForImageRasterTask(
    internal::WorkerPoolTask* image_task,
    const internal::Task::Vector& decode_tasks,
    unsigned priority,
    bool is_required_for_activation,
    internal::GraphNode* raster_required_for_activation_finished_node,
    internal::GraphNode* raster_finished_node,
    TaskGraph* graph) {
  internal::GraphNode* image_node =
      CreateGraphNodeForRasterTask(image_task, decode_tasks, priority, graph);

  if (is_required_for_activation) {
    raster_required_for_activation_finished_node->add_dependency();
    image_node->add_dependent(raster_required_for_activation_finished_node);
  }

  raster_finished_node->add_dependency();
  image_node->add_dependent(raster_finished_node);
}

}  // namespace cc
