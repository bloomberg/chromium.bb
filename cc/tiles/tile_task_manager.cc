// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_task_manager.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"

namespace cc {

TileTaskManager::TileTaskManager() {}

TileTaskManager::~TileTaskManager() {}

// static
std::unique_ptr<TileTaskManagerImpl> TileTaskManagerImpl::Create(
    std::unique_ptr<RasterBufferProvider> raster_buffer_provider,
    TaskGraphRunner* task_graph_runner) {
  return base::WrapUnique<TileTaskManagerImpl>(new TileTaskManagerImpl(
      std::move(raster_buffer_provider), task_graph_runner));
}

TileTaskManagerImpl::TileTaskManagerImpl(
    std::unique_ptr<RasterBufferProvider> raster_buffer_provider,
    TaskGraphRunner* task_graph_runner)
    : raster_buffer_provider_(std::move(raster_buffer_provider)),
      task_graph_runner_(task_graph_runner),
      namespace_token_(task_graph_runner->GetNamespaceToken()) {}

TileTaskManagerImpl::~TileTaskManagerImpl() {}

void TileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::ScheduleTasks");

  raster_buffer_provider_->OrderingBarrier();
  task_graph_runner_->ScheduleTasks(namespace_token_, graph);
}

void TileTaskManagerImpl::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::CheckForCompletedTasks");
  Task::Vector completed_tasks;
  task_graph_runner_->CollectCompletedTasks(namespace_token_, &completed_tasks);

  for (auto& task : completed_tasks) {
    DCHECK(task->state().IsFinished() || task->state().IsCanceled());
    TileTask* tile_task = static_cast<TileTask*>(task.get());
    tile_task->OnTaskCompleted();
    tile_task->DidComplete();
  }
}

void TileTaskManagerImpl::Shutdown() {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::Shutdown");

  // Cancel non-scheduled tasks and wait for running tasks to finish.
  TaskGraph empty;
  task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
  task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);

  raster_buffer_provider_->Shutdown();
}

RasterBufferProvider* TileTaskManagerImpl::GetRasterBufferProvider() const {
  return raster_buffer_provider_.get();
}

}  // namespace cc
