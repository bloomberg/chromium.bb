// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <deque>
#include <limits>

#include "base/lazy_instance.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/rasterizer.h"

namespace cc {

namespace {

class FakeRasterizerImpl : public Rasterizer, public RasterizerTaskClient {
 public:
  // Overridden from Rasterizer:
  void SetClient(RasterizerClient* client) override {}
  void Shutdown() override {}
  void ScheduleTasks(RasterTaskQueue* queue) override {
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

  // Overridden from RasterizerTaskClient:
  scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource) override {
    return nullptr;
  }
  void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) override {}

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
                  NULL,
                  std::numeric_limits<size_t>::max()) {
}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourcePool* resource_pool)
    : TileManager(client,
                  base::MessageLoopProxy::current(),
                  resource_pool,
                  g_fake_rasterizer.Pointer(),
                  NULL,
                  std::numeric_limits<size_t>::max()) {
}

FakeTileManager::~FakeTileManager() {}

void FakeTileManager::AssignMemoryToTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  tiles_for_raster.clear();

  SetGlobalStateForTesting(state);
  AssignGpuMemoryToTiles(&tiles_for_raster);
}

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
