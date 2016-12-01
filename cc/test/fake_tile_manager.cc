// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_tile_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <limits>

#include "base/lazy_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/raster/raster_buffer.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/test/fake_tile_task_manager.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

namespace {

base::LazyInstance<SynchronousTaskGraphRunner> g_synchronous_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<FakeRasterBufferProviderImpl> g_fake_raster_buffer_provider =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  base::ThreadTaskRunnerHandle::Get().get(),
                  std::numeric_limits<size_t>::max(),
                  false /* use_partial_raster */),
      image_decode_cache_(
          ResourceFormat::RGBA_8888,
          LayerTreeSettings().software_decoded_image_budget_bytes) {
  SetResources(
      nullptr, &image_decode_cache_, g_synchronous_task_graph_runner.Pointer(),
      g_fake_raster_buffer_provider.Pointer(),
      std::numeric_limits<size_t>::max(), false /* use_gpu_rasterization */);
  SetTileTaskManagerForTesting(base::MakeUnique<FakeTileTaskManagerImpl>());
}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourcePool* resource_pool)
    : TileManager(client,
                  base::ThreadTaskRunnerHandle::Get().get(),
                  std::numeric_limits<size_t>::max(),
                  false /* use_partial_raster */),
      image_decode_cache_(
          ResourceFormat::RGBA_8888,
          LayerTreeSettings().software_decoded_image_budget_bytes) {
  SetResources(resource_pool, &image_decode_cache_,
               g_synchronous_task_graph_runner.Pointer(),
               g_fake_raster_buffer_provider.Pointer(),
               std::numeric_limits<size_t>::max(),
               false /* use_gpu_rasterization */);
  SetTileTaskManagerForTesting(base::MakeUnique<FakeTileTaskManagerImpl>());
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
