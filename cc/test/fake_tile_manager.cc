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
  FakeRasterWorkerPool() : RasterWorkerPool(NULL, 1) {}

  virtual void ScheduleTasks(RasterTask::Queue* queue) OVERRIDE {
    RasterWorkerPool::SetRasterTasks(queue);
    for (RasterTaskVector::const_iterator it = raster_tasks().begin();
         it != raster_tasks().end(); ++it) {
      completed_tasks_.push_back(it->get());
    }
  }
  virtual void CheckForCompletedTasks() OVERRIDE {
    while (!completed_tasks_.empty()) {
      internal::RasterWorkerPoolTask* task = completed_tasks_.front().get();
      task->WillComplete();
      task->CompleteOnOriginThread();
      task->DidComplete();
      completed_tasks_.pop_front();
    }
  }
  virtual GLenum GetResourceTarget() const OVERRIDE {
    return GL_TEXTURE_2D;
  }
  virtual ResourceFormat GetResourceFormat() const OVERRIDE {
    return RGBA_8888;
  }
  virtual void OnRasterTasksFinished() OVERRIDE {}
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE {}

 private:
  typedef std::deque<scoped_refptr<internal::RasterWorkerPoolTask> > TaskDeque;
  TaskDeque completed_tasks_;
};

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  NULL,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  1,
                  std::numeric_limits<unsigned>::max(),
                  NULL) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider)
    : TileManager(client,
                  resource_provider,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  1,
                  std::numeric_limits<unsigned>::max(),
                  NULL) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider,
                                 size_t raster_task_limit_bytes)
    : TileManager(client,
                  resource_provider,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  1,
                  raster_task_limit_bytes,
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

void FakeTileManager::CheckForCompletedTasks() {
  RasterWorkerPoolForTesting()->CheckForCompletedTasks();
}

void FakeTileManager::Release(Tile* tile) {
  TileManager::Release(tile);
  CleanUpReleasedTiles();
}

}  // namespace cc
