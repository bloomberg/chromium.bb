// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <deque>
#include <limits>

#include "base/lazy_instance.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/tile_task_runner.h"

namespace cc {

namespace {

class FakeTileTaskRunnerImpl : public TileTaskRunner, public TileTaskClient {
 public:
  // Overridden from TileTaskRunner:
  void SetClient(TileTaskRunnerClient* client) override {}
  void Shutdown() override {}
  void ScheduleTasks(TileTaskQueue* queue) override {
    for (TileTaskQueue::Item::Vector::const_iterator it = queue->items.begin();
         it != queue->items.end(); ++it) {
      RasterTask* task = it->task;

      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();

      completed_tasks_.push_back(task);
    }
  }
  void CheckForCompletedTasks() override {
    for (RasterTask::Vector::iterator it = completed_tasks_.begin();
         it != completed_tasks_.end();
         ++it) {
      RasterTask* task = it->get();

      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      task->RunReplyOnOriginThread();
    }
    completed_tasks_.clear();
  }
  ResourceFormat GetResourceFormat() override { return RGBA_8888; }

  // Overridden from TileTaskClient:
  scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource) override {
    return nullptr;
  }
  void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) override {}

 private:
  RasterTask::Vector completed_tasks_;
};
base::LazyInstance<FakeTileTaskRunnerImpl> g_fake_tile_task_runner =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  base::MessageLoopProxy::current(),
                  nullptr,
                  g_fake_tile_task_runner.Pointer(),
                  nullptr,
                  std::numeric_limits<size_t>::max()) {
}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourcePool* resource_pool)
    : TileManager(client,
                  base::MessageLoopProxy::current(),
                  resource_pool,
                  g_fake_tile_task_runner.Pointer(),
                  nullptr,
                  std::numeric_limits<size_t>::max()) {
}

FakeTileManager::~FakeTileManager() {}

bool FakeTileManager::HasBeenAssignedMemory(Tile* tile) {
  return std::find(tiles_for_raster.begin(),
                   tiles_for_raster.end(),
                   tile) != tiles_for_raster.end();
}

void FakeTileManager::Release(Tile* tile) {
  TileManager::Release(tile);

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();
}

}  // namespace cc
