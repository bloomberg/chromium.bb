// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include "cc/resources/raster_worker_pool.h"

namespace cc {

namespace {

class FakeRasterWorkerPool : public RasterWorkerPool {
 public:
  FakeRasterWorkerPool() : RasterWorkerPool(NULL, 1) {}

  virtual void ScheduleTasks(RasterTask::Queue* queue) OVERRIDE {}
  virtual void OnRasterTasksFinished() OVERRIDE {}
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE {}
};

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  NULL,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  1,
                  NULL,
                  GL_RGBA) {}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourceProvider* resource_provider)
    : TileManager(client,
                  resource_provider,
                  make_scoped_ptr<RasterWorkerPool>(new FakeRasterWorkerPool),
                  1,
                  NULL,
                  resource_provider->best_texture_format()) {}

void FakeTileManager::ReassignMemoryToOOMTilesRequiredForActivation() {
  ReassignGpuMemoryToOOMTilesRequiredForActivation();
}

bool FakeTileManager::HasBeenAssignedMemory(Tile* tile) {
  return std::find(tiles_that_need_to_be_rasterized().begin(),
                   tiles_that_need_to_be_rasterized().end(),
                   tile) != tiles_that_need_to_be_rasterized().end();
}

}  // namespace cc
