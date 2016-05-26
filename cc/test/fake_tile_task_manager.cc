// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_task_manager.h"

#include "base/memory/ptr_util.h"

namespace cc {

FakeTileTaskManagerImpl::FakeTileTaskManagerImpl()
    : raster_buffer_provider_(base::WrapUnique<RasterBufferProvider>(
          new FakeRasterBufferProviderImpl)) {}

FakeTileTaskManagerImpl::FakeTileTaskManagerImpl(
    std::unique_ptr<RasterBufferProvider> raster_buffer_provider)
    : raster_buffer_provider_(std::move(raster_buffer_provider)) {}

FakeTileTaskManagerImpl::~FakeTileTaskManagerImpl() {
  DCHECK_EQ(0u, completed_tasks_.size());
}

void FakeTileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  for (const auto& node : graph->nodes) {
    TileTask* task = static_cast<TileTask*>(node.task);
    // Cancel the task and append to |completed_tasks_|.
    task->state().DidCancel();
    completed_tasks_.push_back(task);
  }
}

void FakeTileTaskManagerImpl::CheckForCompletedTasks() {
  for (auto& task : completed_tasks_) {
    DCHECK(task->state().IsFinished() || task->state().IsCanceled());
    TileTask* tile_task = static_cast<TileTask*>(task.get());
    tile_task->OnTaskCompleted();
  }

  completed_tasks_.clear();
}

void FakeTileTaskManagerImpl::Shutdown() {}

RasterBufferProvider* FakeTileTaskManagerImpl::GetRasterBufferProvider() const {
  return raster_buffer_provider_.get();
}

}  // namespace cc
