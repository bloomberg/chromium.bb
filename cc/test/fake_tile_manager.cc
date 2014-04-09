// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <deque>
#include <limits>

#include "base/lazy_instance.h"
#include "cc/resources/raster_worker_pool.h"

namespace cc {

namespace {

class FakeRasterWorkerPool : public RasterWorkerPool,
                             public internal::WorkerPoolTaskClient {
 public:
  // Overridden from RasterWorkerPool:
  virtual void SetClient(RasterWorkerPoolClient* client) OVERRIDE {}
  virtual void Shutdown() OVERRIDE {}
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE {
    for (RasterTaskQueue::Item::Vector::const_iterator it =
             queue->items.begin();
         it != queue->items.end();
         ++it) {
      internal::RasterWorkerPoolTask* task = it->task;

      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();

      completed_tasks_.push_back(task);
    }
  }
  virtual void CheckForCompletedTasks() OVERRIDE {
    for (internal::WorkerPoolTask::Vector::iterator it =
             completed_tasks_.begin();
         it != completed_tasks_.end();
         ++it) {
      internal::WorkerPoolTask* task = it->get();

      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      task->RunReplyOnOriginThread();
    }
    completed_tasks_.clear();
  }
  virtual GLenum GetResourceTarget() const OVERRIDE {
    return GL_TEXTURE_2D;
  }
  virtual ResourceFormat GetResourceFormat() const OVERRIDE {
    return RGBA_8888;
  }

  // Overridden from internal::WorkerPoolTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(internal::WorkerPoolTask* task,
                                           const Resource* resource) OVERRIDE {
    return NULL;
  }
  virtual void ReleaseCanvasForRaster(internal::WorkerPoolTask* task,
                                      const Resource* resource) OVERRIDE {}

 private:
  internal::WorkerPoolTask::Vector completed_tasks_;
};
base::LazyInstance<FakeRasterWorkerPool> g_fake_raster_worker_pool =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  NULL,
                  g_fake_raster_worker_pool.Pointer(),
                  g_fake_raster_worker_pool.Pointer(),
                  std::numeric_limits<unsigned>::max(),
                  true,
                  NULL) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider)
    : TileManager(client,
                  resource_provider,
                  g_fake_raster_worker_pool.Pointer(),
                  g_fake_raster_worker_pool.Pointer(),
                  std::numeric_limits<unsigned>::max(),
                  true,
                  NULL) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider,
                                 bool allow_on_demand_raster)
    : TileManager(client,
                  resource_provider,
                  g_fake_raster_worker_pool.Pointer(),
                  g_fake_raster_worker_pool.Pointer(),
                  std::numeric_limits<unsigned>::max(),
                  allow_on_demand_raster,
                  NULL) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider,
                                 size_t raster_task_limit_bytes)
    : TileManager(client,
                  resource_provider,
                  g_fake_raster_worker_pool.Pointer(),
                  g_fake_raster_worker_pool.Pointer(),
                  raster_task_limit_bytes,
                  true,
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
