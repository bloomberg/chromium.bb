// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/gpu_raster_worker_pool.h"

#include "base/debug/trace_event.h"
#include "cc/output/context_provider.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_gpu_raster.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {

// static
scoped_ptr<RasterWorkerPool> GpuRasterWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    ContextProvider* context_provider,
    ResourceProvider* resource_provider) {
  return make_scoped_ptr<RasterWorkerPool>(new GpuRasterWorkerPool(
      task_runner, context_provider, resource_provider));
}

GpuRasterWorkerPool::GpuRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                                         ContextProvider* context_provider,
                                         ResourceProvider* resource_provider)
    : task_runner_(task_runner),
      task_graph_runner_(new TaskGraphRunner),
      namespace_token_(task_graph_runner_->GetNamespaceToken()),
      context_provider_(context_provider),
      resource_provider_(resource_provider),
      run_tasks_on_origin_thread_pending_(false),
      raster_tasks_pending_(false),
      raster_tasks_required_for_activation_pending_(false),
      raster_finished_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
}

GpuRasterWorkerPool::~GpuRasterWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

Rasterizer* GpuRasterWorkerPool::AsRasterizer() {
  return this;
}

void GpuRasterWorkerPool::SetClient(RasterizerClient* client) {
  client_ = client;
}

void GpuRasterWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void GpuRasterWorkerPool::ScheduleTasks(RasterTaskQueue* queue) {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::ScheduleTasks");

  DCHECK_EQ(queue->required_for_activation_count,
            static_cast<size_t>(
                std::count_if(queue->items.begin(),
                              queue->items.end(),
                              RasterTaskQueue::Item::IsRequiredForActivation)));

  raster_tasks_pending_ = true;
  raster_tasks_required_for_activation_pending_ = true;

  unsigned priority = kRasterTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnRasterFinished callbacks.
  raster_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<RasterizerTask>
      new_raster_required_for_activation_finished_task(
          CreateRasterRequiredForActivationFinishedTask(
              queue->required_for_activation_count,
              task_runner_.get(),
              base::Bind(
                  &GpuRasterWorkerPool::OnRasterRequiredForActivationFinished,
                  raster_finished_weak_ptr_factory_.GetWeakPtr())));
  scoped_refptr<RasterizerTask> new_raster_finished_task(
      CreateRasterFinishedTask(
          task_runner_.get(),
          base::Bind(&GpuRasterWorkerPool::OnRasterFinished,
                     raster_finished_weak_ptr_factory_.GetWeakPtr())));

  for (RasterTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end();
       ++it) {
    const RasterTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    if (item.required_for_activation) {
      graph_.edges.push_back(TaskGraph::Edge(
          task, new_raster_required_for_activation_finished_task.get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);

    graph_.edges.push_back(
        TaskGraph::Edge(task, new_raster_finished_task.get()));
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

  ScheduleRunTasksOnOriginThread();

  raster_finished_task_ = new_raster_finished_task;
  raster_required_for_activation_finished_task_ =
      new_raster_required_for_activation_finished_task;
}

void GpuRasterWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::CheckForCompletedTasks");

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
}

SkCanvas* GpuRasterWorkerPool::AcquireCanvasForRaster(RasterTask* task) {
  return resource_provider_->MapGpuRasterBuffer(task->resource()->id());
}

void GpuRasterWorkerPool::ReleaseCanvasForRaster(RasterTask* task) {
  resource_provider_->UnmapGpuRasterBuffer(task->resource()->id());
}

void GpuRasterWorkerPool::OnRasterFinished() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::OnRasterFinished");

  DCHECK(raster_tasks_pending_);
  raster_tasks_pending_ = false;
  client_->DidFinishRunningTasks();
}

void GpuRasterWorkerPool::OnRasterRequiredForActivationFinished() {
  TRACE_EVENT0("cc",
               "GpuRasterWorkerPool::OnRasterRequiredForActivationFinished");

  DCHECK(raster_tasks_required_for_activation_pending_);
  raster_tasks_required_for_activation_pending_ = false;
  client_->DidFinishRunningTasksRequiredForActivation();
}

void GpuRasterWorkerPool::ScheduleRunTasksOnOriginThread() {
  if (run_tasks_on_origin_thread_pending_)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GpuRasterWorkerPool::RunTasksOnOriginThread,
                 weak_ptr_factory_.GetWeakPtr()));
  run_tasks_on_origin_thread_pending_ = true;
}

void GpuRasterWorkerPool::RunTasksOnOriginThread() {
  TRACE_EVENT0("cc", "GpuRasterWorkerPool::RunTasksOnOriginThread");

  DCHECK(run_tasks_on_origin_thread_pending_);
  run_tasks_on_origin_thread_pending_ = false;

  ScopedGpuRaster gpu_raster(context_provider_);
  task_graph_runner_->RunUntilIdle();
}

}  // namespace cc
