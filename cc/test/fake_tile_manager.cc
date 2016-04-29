// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <limits>

#include "base/lazy_instance.h"
#include "base/thread_task_runner_handle.h"
#include "cc/raster/raster_buffer.h"
#include "cc/test/fake_tile_task_manager.h"

namespace cc {

namespace {

base::LazyInstance<FakeTileTaskManagerImpl> g_fake_tile_task_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  base::ThreadTaskRunnerHandle::Get(),
                  std::numeric_limits<size_t>::max(),
                  false /* use_partial_raster */) {
  SetResources(
      nullptr, &image_decode_controller_, g_fake_tile_task_manager.Pointer(),
      std::numeric_limits<size_t>::max(), false /* use_gpu_rasterization */);
}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourcePool* resource_pool)
    : TileManager(client,
                  base::ThreadTaskRunnerHandle::Get(),
                  std::numeric_limits<size_t>::max(),
                  false /* use_partial_raster */) {
  SetResources(resource_pool, &image_decode_controller_,
               g_fake_tile_task_manager.Pointer(),
               std::numeric_limits<size_t>::max(),
               false /* use_gpu_rasterization */);
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
