// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <deque>
#include <limits>

#include "cc/resources/raster_worker_pool.h"

namespace cc {

namespace {

class FakeRasterWorkerPool : public RasterWorkerPool {
 public:
  FakeRasterWorkerPool() : RasterWorkerPool(NULL, NULL, NULL) {}

  // Overridden from RasterWorkerPool:
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
    while (!completed_tasks_.empty()) {
      internal::WorkerPoolTask* task = completed_tasks_.front().get();

      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();

      task->RunReplyOnOriginThread();

      completed_tasks_.pop_front();
    }
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
  // Overridden from RasterWorkerPool:
  virtual void OnRasterTasksFinished() OVERRIDE {}
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE {}

  TaskDeque completed_tasks_;
};

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  base::MessageLoopProxy::current().get(),
                  NULL,
                  NULL,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  std::numeric_limits<unsigned>::max(),
                  NULL,
                  true) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider)
    : TileManager(client,
                  base::MessageLoopProxy::current().get(),
                  resource_provider,
                  NULL,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  std::numeric_limits<unsigned>::max(),
                  NULL,
                  true) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider,
                                 bool allow_on_demand_raster)
    : TileManager(client,
                  base::MessageLoopProxy::current().get(),
                  resource_provider,
                  NULL,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  std::numeric_limits<unsigned>::max(),
                  NULL,
                  allow_on_demand_raster) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider,
                                 size_t raster_task_limit_bytes)
    : TileManager(client,
                  base::MessageLoopProxy::current().get(),
                  resource_provider,
                  NULL,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  raster_task_limit_bytes,
                  NULL,
                  true) {}

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
