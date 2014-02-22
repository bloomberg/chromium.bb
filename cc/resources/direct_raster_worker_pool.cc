// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/direct_raster_worker_pool.h"

#include "cc/output/context_provider.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

// static
scoped_ptr<RasterWorkerPool> DirectRasterWorkerPool::Create(
    ResourceProvider* resource_provider,
    ContextProvider* context_provider) {
  return make_scoped_ptr<RasterWorkerPool>(
      new DirectRasterWorkerPool(resource_provider, context_provider));
}

DirectRasterWorkerPool::DirectRasterWorkerPool(
    ResourceProvider* resource_provider,
    ContextProvider* context_provider)
    : RasterWorkerPool(NULL, resource_provider),
      context_provider_(context_provider),
      run_tasks_on_origin_thread_pending_(false),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false),
      weak_factory_(this) {}

DirectRasterWorkerPool::~DirectRasterWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

void DirectRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

  raster_tasks_pending_ = true;
  raster_tasks_required_for_activation_pending_ = true;

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              queue->required_for_activation_count));
  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task(
      CreateRasterFinishedTask());

  // Need to cancel tasks not present in new queue if we haven't had a
  // chance to run the previous set of tasks yet.
  // TODO(reveman): Remove this once only tasks for which
  // ::ScheduleOnOriginThread has been called need to be canceled.
  if (run_tasks_on_origin_thread_pending_) {
    for (RasterTaskQueue::Item::Vector::const_iterator it =
             raster_tasks_.items.begin();
         it != raster_tasks_.items.end();
         ++it) {
      internal::RasterWorkerPoolTask* task = it->task;

      if (std::find_if(queue->items.begin(),
                       queue->items.end(),
                       RasterTaskQueue::Item::TaskComparator(task)) ==
          queue->items.end())
        completed_tasks_.push_back(task);
    }
  }

  ScheduleRunTasksOnOriginThread();

  raster_tasks_.Swap(queue);

  set_raster_finished_task(new_raster_finished_task);
  set_raster_required_for_activation_finished_task(
      new_raster_required_for_activation_finished_task);
}

unsigned DirectRasterWorkerPool::GetResourceTarget() const {
  return GL_TEXTURE_2D;
}

ResourceFormat DirectRasterWorkerPool::GetResourceFormat() const {
  return resource_provider()->best_texture_format();
}

void DirectRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::CheckForCompletedTasks");

  for (internal::WorkerPoolTask::Vector::const_iterator it =
           completed_tasks_.begin();
       it != completed_tasks_.end();
       ++it) {
    internal::WorkerPoolTask* task = it->get();

    task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

SkCanvas* DirectRasterWorkerPool::AcquireCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  return resource_provider()->MapDirectRasterBuffer(resource->id());
}

void DirectRasterWorkerPool::ReleaseCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  resource_provider()->UnmapDirectRasterBuffer(resource->id());
}

void DirectRasterWorkerPool::OnRasterTasksFinished() {
  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  client()->DidFinishRunningTasks();
}

void DirectRasterWorkerPool::OnRasterTasksRequiredForActivationFinished() {
  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  client()->DidFinishRunningTasksRequiredForActivation();
}

void DirectRasterWorkerPool::ScheduleRunTasksOnOriginThread() {
  if (run_tasks_on_origin_thread_pending_)
    return;

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&DirectRasterWorkerPool::RunTasksOnOriginThread,
                 weak_factory_.GetWeakPtr()));
  run_tasks_on_origin_thread_pending_ = true;
}

void DirectRasterWorkerPool::RunTasksOnOriginThread() {
  DCHECK(run_tasks_on_origin_thread_pending_);
  run_tasks_on_origin_thread_pending_ = false;

  if (!raster_tasks_.items.empty()) {
    GrContext* gr_context = context_provider_->GrContext();
    // TODO(alokp): Implement TestContextProvider::GrContext().
    if (gr_context)
      gr_context->resetContext();

    for (RasterTaskQueue::Item::Vector::const_iterator it =
             raster_tasks_.items.begin();
         it != raster_tasks_.items.end();
         ++it) {
      internal::RasterWorkerPoolTask* task = it->task;
      DCHECK(!task->HasCompleted());

      // First need to run all dependencies.
      for (internal::WorkerPoolTask::Vector::const_iterator it =
               task->dependencies().begin();
           it != task->dependencies().end();
           ++it) {
        internal::WorkerPoolTask* dependency = it->get();

        if (dependency->HasCompleted())
          continue;

        RunTaskOnOriginThread(dependency);

        completed_tasks_.push_back(dependency);
      }

      RunTaskOnOriginThread(task);

      completed_tasks_.push_back(task);
    }

    // TODO(alokp): Implement TestContextProvider::GrContext().
    if (gr_context)
      gr_context->flush();
  }

  RunTaskOnOriginThread(raster_required_for_activation_finished_task());
  RunTaskOnOriginThread(raster_finished_task());
}

}  // namespace cc
