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

TileTaskManagerImpl::~TileTaskManagerImpl() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

void TileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::ScheduleTasks");

  for (TaskGraph::Node::Vector::iterator it = graph->nodes.begin();
       it != graph->nodes.end(); ++it) {
    TaskGraph::Node& node = *it;
    TileTask* task = static_cast<TileTask*>(node.task);

    if (!task->HasBeenScheduled()) {
      task->WillSchedule();
      task->ScheduleOnOriginThread(raster_buffer_provider_.get());
      task->DidSchedule();
    }
  }

  raster_buffer_provider_->OrderingBarrier();

  task_graph_runner_->ScheduleTasks(namespace_token_, graph);
}

void TileTaskManagerImpl::CheckForCompletedTasks() {
  TRACE_EVENT0("cc", "TileTaskManagerImpl::CheckForCompletedTasks");

  task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                            &completed_tasks_);
  for (Task::Vector::const_iterator it = completed_tasks_.begin();
       it != completed_tasks_.end(); ++it) {
    TileTask* task = static_cast<TileTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(raster_buffer_provider_.get());
    task->DidComplete();
  }
  completed_tasks_.clear();
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
