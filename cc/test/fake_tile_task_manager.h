// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_TILE_TASK_MANAGER_H_
#define CC_TEST_FAKE_TILE_TASK_MANAGER_H_

#include "cc/test/fake_raster_buffer_provider.h"
#include "cc/tiles/tile_task_manager.h"

namespace cc {

// Fake TileTaskManager.
class FakeTileTaskManagerImpl : public TileTaskManager {
 public:
  FakeTileTaskManagerImpl();
  // Ctor for custom raster buffer provider.
  FakeTileTaskManagerImpl(
      std::unique_ptr<RasterBufferProvider> raster_buffer_provider);
  ~FakeTileTaskManagerImpl() override;

  // Overridden from TileTaskManager:
  void ScheduleTasks(TaskGraph* graph) override;
  void CheckForCompletedTasks() override;
  void Shutdown() override;
  RasterBufferProvider* GetRasterBufferProvider() const override;

 protected:
  std::unique_ptr<RasterBufferProvider> raster_buffer_provider_;
  Task::Vector completed_tasks_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_TILE_TASK_MANAGER_H_
