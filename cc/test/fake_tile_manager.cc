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
#include "cc/raster/tile_task_runner.h"

namespace cc {

namespace {

class FakeTileTaskRunnerImpl : public TileTaskRunner,
                               public RasterBufferProvider {
 public:
  // Overridden from TileTaskRunner:
  void Shutdown() override {}
  void ScheduleTasks(TaskGraph* graph) override {
    for (const auto& node : graph->nodes) {
      TileTask* task = static_cast<TileTask*>(node.task);

      task->WillSchedule();
      task->ScheduleOnOriginThread(this);
      task->DidSchedule();

      completed_tasks_.push_back(task);
    }
  }
  void CheckForCompletedTasks() override {
    for (TileTask::Vector::iterator it = completed_tasks_.begin();
         it != completed_tasks_.end(); ++it) {
      TileTask* task = it->get();

      task->WillComplete();
      task->CompleteOnOriginThread(this);
      task->DidComplete();
    }
    completed_tasks_.clear();
  }
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override {
    return RGBA_8888;
  }
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override {
    return ResourceFormatRequiresSwizzle(GetResourceFormat(must_support_alpha));
  }
  RasterBufferProvider* AsRasterBufferProvider() override { return this; }

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override {
    return nullptr;
  }
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override {}

 private:
  TileTask::Vector completed_tasks_;
};
base::LazyInstance<FakeTileTaskRunnerImpl> g_fake_tile_task_runner =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FakeTileManager::FakeTileManager(TileManagerClient* client)
    : TileManager(client,
                  base::ThreadTaskRunnerHandle::Get(),
                  std::numeric_limits<size_t>::max(),
                  false /* use_partial_raster */) {
  SetResources(nullptr, g_fake_tile_task_runner.Pointer(),
               &image_decode_controller_, std::numeric_limits<size_t>::max(),
               false /* use_gpu_rasterization */);
}

FakeTileManager::FakeTileManager(TileManagerClient* client,
                                 ResourcePool* resource_pool)
    : TileManager(client,
                  base::ThreadTaskRunnerHandle::Get(),
                  std::numeric_limits<size_t>::max(),
                  false /* use_partial_raster */) {
  SetResources(resource_pool, g_fake_tile_task_runner.Pointer(),
               &image_decode_controller_, std::numeric_limits<size_t>::max(),
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
