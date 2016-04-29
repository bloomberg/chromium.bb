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

FakeTileTaskManagerImpl::~FakeTileTaskManagerImpl() {}

void FakeTileTaskManagerImpl::ScheduleTasks(TaskGraph* graph) {
  for (const auto& node : graph->nodes) {
    TileTask* task = static_cast<TileTask*>(node.task);

    task->WillSchedule();
    task->ScheduleOnOriginThread(raster_buffer_provider_.get());
    task->DidSchedule();

    completed_tasks_.push_back(task);
  }
}

void FakeTileTaskManagerImpl::CheckForCompletedTasks() {
  for (Task::Vector::iterator it = completed_tasks_.begin();
       it != completed_tasks_.end(); ++it) {
    TileTask* task = static_cast<TileTask*>(it->get());

    task->WillComplete();
    task->CompleteOnOriginThread(raster_buffer_provider_.get());
    task->DidComplete();
  }
  completed_tasks_.clear();
}

void FakeTileTaskManagerImpl::Shutdown() {}

RasterBufferProvider* FakeTileTaskManagerImpl::GetRasterBufferProvider() const {
  return raster_buffer_provider_.get();
}

}  // namespace cc
