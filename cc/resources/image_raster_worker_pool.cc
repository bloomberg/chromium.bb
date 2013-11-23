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

namespace {

class ImageWorkerPoolTaskImpl : public internal::WorkerPoolTask {
 public:
  typedef base::Callback<void(bool was_canceled)> Reply;

  ImageWorkerPoolTaskImpl(internal::RasterWorkerPoolTask* task,
                          uint8_t* buffer,
                          int stride,
                          const Reply& reply)
      : task_(task),
        buffer_(buffer),
        stride_(stride),
        reply_(reply) {
  }

  // Overridden from internal::WorkerPoolTask:
  virtual void RunOnWorkerThread(unsigned thread_index) OVERRIDE {
    TRACE_EVENT0("cc", "ImageWorkerPoolTaskImpl::RunOnWorkerThread");
    if (!buffer_)
      return;

    task_->RunOnWorkerThread(thread_index,
                             buffer_,
                             task_->resource()->size(),
                             stride_);
  }
  virtual void CompleteOnOriginThread() OVERRIDE {
    reply_.Run(!HasFinishedRunning());
  }

 private:
  virtual ~ImageWorkerPoolTaskImpl() {}

  scoped_refptr<internal::RasterWorkerPoolTask> task_;
  uint8_t* buffer_;
  int stride_;
  const Reply reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageWorkerPoolTaskImpl);
};

}  // namespace

ImageRasterWorkerPool::ImageRasterWorkerPool(
    ResourceProvider* resource_provider,
    size_t num_threads,
    GLenum texture_target)
    : RasterWorkerPool(resource_provider, num_threads),
      texture_target_(texture_target),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false) {
}

ImageRasterWorkerPool::~ImageRasterWorkerPool() {
  DCHECK_EQ(0u, image_tasks_.size());
}

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
          CreateRasterRequiredForActivationFinishedTask());
  internal::GraphNode* raster_required_for_activation_finished_node =
      CreateGraphNodeForTask(
          new_raster_required_for_activation_finished_task.get(),
          priority++,
          &graph);

  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task(
      CreateRasterFinishedTask());
  internal::GraphNode* raster_finished_node =
      CreateGraphNodeForTask(new_raster_finished_task.get(),
                             priority++,
                             &graph);

  for (RasterTaskVector::const_iterator it = raster_tasks().begin();
       it != raster_tasks().end(); ++it) {
    internal::RasterWorkerPoolTask* task = it->get();
    DCHECK(!task->HasCompleted());
    DCHECK(!task->WasCanceled());

    TaskMap::iterator image_it = image_tasks_.find(task);
    if (image_it != image_tasks_.end()) {
      internal::WorkerPoolTask* image_task = image_it->second.get();
      CreateGraphNodeForImageTask(
          image_task,
          task->dependencies(),
          priority++,
          IsRasterTaskRequiredForActivation(task),
          raster_required_for_activation_finished_node,
          raster_finished_node,
          &graph);
      continue;
    }

    // Acquire image for resource.
    resource_provider()->AcquireImage(task->resource()->id());

    // Map image for raster.
    uint8* buffer = resource_provider()->MapImage(task->resource()->id());
    int stride = resource_provider()->GetImageStride(task->resource()->id());

    scoped_refptr<internal::WorkerPoolTask> new_image_task(
        new ImageWorkerPoolTaskImpl(
            task,
            buffer,
            stride,
            base::Bind(&ImageRasterWorkerPool::OnRasterTaskCompleted,
                       base::Unretained(this),
                       make_scoped_refptr(task))));
    image_tasks_[task] = new_image_task;
    CreateGraphNodeForImageTask(
        new_image_task.get(),
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

  TRACE_EVENT_ASYNC_STEP_INTO1(
      "cc", "ScheduledTasks", this, "rasterizing",
      "state", TracedValue::FromValue(StateAsValue().release()));
}

GLenum ImageRasterWorkerPool::GetResourceTarget() const {
  return texture_target_;
}

ResourceFormat ImageRasterWorkerPool::GetResourceFormat() const {
  return resource_provider()->best_texture_format();
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
      "cc", "ScheduledTasks", this, "rasterizing",
      "state", TracedValue::FromValue(StateAsValue().release()));
  client()->DidFinishRunningTasksRequiredForActivation();
}

void ImageRasterWorkerPool::OnRasterTaskCompleted(
    scoped_refptr<internal::RasterWorkerPoolTask> task,
    bool was_canceled) {
  TRACE_EVENT1("cc", "ImageRasterWorkerPool::OnRasterTaskCompleted",
               "was_canceled", was_canceled);

  DCHECK(image_tasks_.find(task.get()) != image_tasks_.end());

  // Balanced with MapImage() call in ScheduleTasks().
  resource_provider()->UnmapImage(task->resource()->id());

  task->DidRun(was_canceled);
  task->WillComplete();
  task->CompleteOnOriginThread();
  task->DidComplete();

  image_tasks_.erase(task.get());
}

scoped_ptr<base::Value> ImageRasterWorkerPool::StateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  state->SetBoolean("tasks_required_for_activation_pending",
                    raster_tasks_required_for_activation_pending_);
  state->Set("scheduled_state", ScheduledStateAsValue().release());
  return state.PassAs<base::Value>();
}

// static
void ImageRasterWorkerPool::CreateGraphNodeForImageTask(
    internal::WorkerPoolTask* image_task,
    const TaskVector& decode_tasks,
    unsigned priority,
    bool is_required_for_activation,
    internal::GraphNode* raster_required_for_activation_finished_node,
    internal::GraphNode* raster_finished_node,
    TaskGraph* graph) {
  internal::GraphNode* image_node = CreateGraphNodeForRasterTask(image_task,
                                                                 decode_tasks,
                                                                 priority,
                                                                 graph);

  if (is_required_for_activation) {
    raster_required_for_activation_finished_node->add_dependency();
    image_node->add_dependent(raster_required_for_activation_finished_node);
  }

  raster_finished_node->add_dependency();
  image_node->add_dependent(raster_finished_node);
}

}  // namespace cc
