// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/direct_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

// static
scoped_ptr<DirectRasterWorkerPool> DirectRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    ResourceProvider* resource_provider,
    ContextProvider* context_provider) {
  return make_scoped_ptr(new DirectRasterWorkerPool(
      task_runner, resource_provider, context_provider));
}

DirectRasterWorkerPool::DirectRasterWorkerPool(
    base::SequencedTaskRunner* task_runner,
    ResourceProvider* resource_provider,
    ContextProvider* context_provider)
    : task_runner_(task_runner),
      resource_provider_(resource_provider),
      context_provider_(context_provider),
      run_tasks_on_origin_thread_pending_(false),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false),
      raster_finished_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {}

DirectRasterWorkerPool::~DirectRasterWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

void DirectRasterWorkerPool::SetClient(RasterWorkerPoolClient* client) {
  client_ = client;
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

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<internal::WorkerPoolTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              queue->required_for_activation_count,
              task_runner_.get(),
              base::Bind(&DirectRasterWorkerPool::
                             OnRasterRequiredForActivationFinished,
                         raster_finished_weak_ptr_factory_.GetWeakPtr())));
  scoped_refptr<internal::WorkerPoolTask> new_raster_finished_task(
      CreateRasterFinishedTask(
          task_runner_.get(),
          base::Bind(&DirectRasterWorkerPool::OnRasterFinished,
                     raster_finished_weak_ptr_factory_.GetWeakPtr())));

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

  raster_finished_task_ = new_raster_finished_task;
  raster_required_for_activation_finished_task_ =
      new_raster_required_for_activation_finished_task;
}

unsigned DirectRasterWorkerPool::GetResourceTarget() const {
  return GL_TEXTURE_2D;
}

ResourceFormat DirectRasterWorkerPool::GetResourceFormat() const {
  return resource_provider_->best_texture_format();
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
  return resource_provider_->MapDirectRasterBuffer(resource->id());
}

void DirectRasterWorkerPool::ReleaseCanvasForRaster(
    internal::WorkerPoolTask* task,
    const Resource* resource) {
  resource_provider_->UnmapDirectRasterBuffer(resource->id());
}

void DirectRasterWorkerPool::OnRasterFinished() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::OnRasterFinished");

  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  client_->DidFinishRunningTasks();
}

void DirectRasterWorkerPool::OnRasterRequiredForActivationFinished() {
  TRACE_EVENT0("cc",
               "DirectRasterWorkerPool::OnRasterRequiredForActivationFinished");

  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  client_->DidFinishRunningTasksRequiredForActivation();
}

void DirectRasterWorkerPool::ScheduleRunTasksOnOriginThread() {
  if (run_tasks_on_origin_thread_pending_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DirectRasterWorkerPool::RunTasksOnOriginThread,
                 weak_ptr_factory_.GetWeakPtr()));
  run_tasks_on_origin_thread_pending_ = true;
}

void DirectRasterWorkerPool::RunTasksOnOriginThread() {
  TRACE_EVENT0("cc", "DirectRasterWorkerPool::RunTasksOnOriginThread");

  DCHECK(run_tasks_on_origin_thread_pending_);
  run_tasks_on_origin_thread_pending_ = false;

  if (!raster_tasks_.items.empty()) {
    DCHECK(context_provider_);
    DCHECK(context_provider_->ContextGL());
    // TODO(alokp): Use a trace macro to push/pop markers.
    // Using push/pop functions directly incurs cost to evaluate function
    // arguments even when tracing is disabled.
    context_provider_->ContextGL()->PushGroupMarkerEXT(
        0, "DirectRasterWorkerPool::RunTasksOnOriginThread");

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

    context_provider_->ContextGL()->PopGroupMarkerEXT();
  }

  RunTaskOnOriginThread(raster_required_for_activation_finished_task_.get());
  RunTaskOnOriginThread(raster_finished_task_.get());
}

void DirectRasterWorkerPool::RunTaskOnOriginThread(
    internal::WorkerPoolTask* task) {
  task->WillSchedule();
  task->ScheduleOnOriginThread(this);
  task->DidSchedule();

  task->WillRun();
  task->RunOnOriginThread();
  task->DidRun();

  task->WillComplete();
  task->CompleteOnOriginThread(this);
  task->DidComplete();
}

}  // namespace cc
