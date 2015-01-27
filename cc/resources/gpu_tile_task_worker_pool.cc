// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/gpu_tile_task_worker_pool.h"

#include <algorithm>

#include "base/trace_event/trace_event.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/resource.h"
#include "cc/resources/scoped_gpu_raster.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
namespace {

class RasterBufferImpl : public RasterBuffer {
 public:
  RasterBufferImpl() {}

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& rect,
                float scale) override {
    // Don't do anything.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RasterBufferImpl);
};

}  // namespace
// static
scoped_ptr<TileTaskWorkerPool> GpuTileTaskWorkerPool::Create(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider) {
  return make_scoped_ptr<TileTaskWorkerPool>(
      new GpuTileTaskWorkerPool(
          task_runner, task_graph_runner, resource_provider));
}

// TODO(hendrikw): This class should be removed.  See crbug.com/444938.
GpuTileTaskWorkerPool::GpuTileTaskWorkerPool(
    base::SequencedTaskRunner* task_runner,
    TaskGraphRunner* task_graph_runner,
    ResourceProvider* resource_provider)
    : task_runner_(task_runner),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner_->GetNamespaceToken()),
      resource_provider_(resource_provider),
      task_set_finished_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
}

GpuTileTaskWorkerPool::~GpuTileTaskWorkerPool() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

TileTaskRunner* GpuTileTaskWorkerPool::AsTileTaskRunner() {
  return this;
}

void GpuTileTaskWorkerPool::SetClient(TileTaskRunnerClient* client) {
  client_ = client;
}

void GpuTileTaskWorkerPool::Shutdown() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::Shutdown");

  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
}

void GpuTileTaskWorkerPool::ScheduleTasks(TileTaskQueue* queue) {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::ScheduleTasks");

  // Mark all task sets as pending.
  tasks_pending_.set();

  unsigned priority = kTileTaskPriorityBase;

  graph_.Reset();

  // Cancel existing OnTaskSetFinished callbacks.
  task_set_finished_weak_ptr_factory_.InvalidateWeakPtrs();

  scoped_refptr<TileTask> new_task_set_finished_tasks[kNumberOfTaskSets];

  size_t task_count[kNumberOfTaskSets] = {0};

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    new_task_set_finished_tasks[task_set] = CreateTaskSetFinishedTask(
        task_runner_.get(),
        base::Bind(&GpuTileTaskWorkerPool::OnTaskSetFinished,
                   task_set_finished_weak_ptr_factory_.GetWeakPtr(), task_set));
  }

  for (TileTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
       it != queue->items.end(); ++it) {
    const TileTaskQueue::Item& item = *it;
    RasterTask* task = item.task;
    DCHECK(!task->HasCompleted());

    for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
      if (!item.task_sets[task_set])
        continue;

      ++task_count[task_set];

      graph_.edges.push_back(
          TaskGraph::Edge(task, new_task_set_finished_tasks[task_set].get()));
    }

    InsertNodesForRasterTask(&graph_, task, task->dependencies(), priority++);
  }

  for (TaskSet task_set = 0; task_set < kNumberOfTaskSets; ++task_set) {
    InsertNodeForTask(&graph_, new_task_set_finished_tasks[task_set].get(),
                      kTaskSetFinishedTaskPriority, task_count[task_set]);
  }

  ScheduleTasksOnOriginThread(this, &graph_);
  task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);

  std::copy(new_task_set_finished_tasks,
            new_task_set_finished_tasks + kNumberOfTaskSets,
            task_set_finished_tasks_);
}

void GpuTileTaskWorkerPool::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "GpuTileTaskWorkerPool::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  CompleteTasks(completed_tasks_);
  completed_tasks_.clear();
}

ResourceFormat GpuTileTaskWorkerPool::GetResourceFormat() {
  return resource_provider_->best_texture_format();
}

void GpuTileTaskWorkerPool::CompleteTasks(const Task::Vector& tasks) {
  for (auto& task : tasks) {
    RasterTask* raster_task = static_cast<RasterTask*>(task.get());

    raster_task->WillComplete();
    raster_task->CompleteOnOriginThread(this);
    raster_task->DidComplete();

    raster_task->RunReplyOnOriginThread();
  }
  completed_tasks_.clear();
}

scoped_ptr<RasterBuffer> GpuTileTaskWorkerPool::AcquireBufferForRaster(
    const Resource* resource) {
  return make_scoped_ptr<RasterBuffer>(new RasterBufferImpl());
}

void GpuTileTaskWorkerPool::ReleaseBufferForRaster(
    scoped_ptr<RasterBuffer> buffer) {
  // Nothing to do here. RasterBufferImpl destructor cleans up after itself.
}

void GpuTileTaskWorkerPool::OnTaskSetFinished(TaskSet task_set) {
  TRACE_EVENT1("cc", "GpuTileTaskWorkerPool::OnTaskSetFinished", "task_set",
               task_set);

  DCHECK(tasks_pending_[task_set]);
  tasks_pending_[task_set] = false;
  client_->DidFinishRunningTileTasks(task_set);
}

}  // namespace cc
