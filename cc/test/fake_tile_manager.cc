// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <deque>

#include "base/lazy_instance.h"
#include "cc/resources/rasterizer.h"

namespace cc {

namespace {

class FakeRasterizerImpl : public Rasterizer, public RasterizerTaskClient {
 public:
  // Overridden from Rasterizer:
  virtual void SetClient(RasterizerClient* client) OVERRIDE {}
  virtual void Shutdown() OVERRIDE {}
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE {
    for (RasterTaskQueue::Item::Vector::const_iterator it =
             queue->items.begin();
         it != queue->items.end();
         ++it) {
      RasterTask* task = it->task;

      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();

      completed_tasks_.push_back(task);
    }
  }
  virtual void CheckForCompletedTasks() OVERRIDE {
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

  // Overridden from RasterizerTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(RasterTask* task) OVERRIDE {
    return NULL;
  }
  virtual void ReleaseCanvasForRaster(RasterTask* task) OVERRIDE {}

 private:
  RasterTask::Vector completed_tasks_;
};
base::LazyInstance<FakeRasterizerImpl> g_fake_rasterizer =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  base::MessageLoopProxy::current(),
                  NULL,
                  g_fake_rasterizer.Pointer(),
                  NULL) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourcePool* resource_pool)
    : TileManager(client,
                  base::MessageLoopProxy::current(),
                  resource_pool,
                  g_fake_rasterizer.Pointer(),
                  NULL) {}

FakeTileManager::~FakeTileManager() {}

void FakeTileManager::AssignMemoryToTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  tiles_for_raster.clear();
  all_tiles.Clear();

  SetGlobalStateForTesting(state);
  GetTilesWithAssignedBins(&all_tiles);
  AssignGpuMemoryToTiles(&all_tiles, &tiles_for_raster);
}

bool FakeTileManager::HasBeenAssignedMemory(Tile* tile) {
  return std::find(tiles_for_raster.begin(),
                   tiles_for_raster.end(),
                   tile) != tiles_for_raster.end();
}

void FakeTileManager::DidFinishRunningTasksForTesting() {
  DidFinishRunningTasks();
}

void FakeTileManager::Release(Tile* tile) {
  TileManager::Release(tile);
  CleanUpReleasedTiles();
}

}  // namespace cc
