// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_GPU_TILE_TASK_WORKER_POOL_H_
#define CC_RASTER_GPU_TILE_TASK_WORKER_POOL_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/raster/tile_task_worker_pool.h"

namespace cc {
class ContextProvider;
class GpuRasterizer;
class ResourceProvider;

class CC_EXPORT GpuTileTaskWorkerPool : public TileTaskWorkerPool,
                                        public RasterBufferProvider {
 public:
  ~GpuTileTaskWorkerPool() override;

  static std::unique_ptr<TileTaskWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ContextProvider* context_provider,
      ResourceProvider* resource_provider,
      bool use_distance_field_text,
      int gpu_rasterization_msaa_sample_count);

  // Overridden from TileTaskWorkerPool:
  void Shutdown() override;
  void ScheduleTasks(TaskGraph* graph) override;
  void CheckForCompletedTasks() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;
  RasterBufferProvider* AsRasterBufferProvider() override;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;

 private:
  GpuTileTaskWorkerPool(base::SequencedTaskRunner* task_runner,
                        TaskGraphRunner* task_graph_runner,
                        ContextProvider* context_provider,
                        ResourceProvider* resource_provider,
                        bool use_distance_field_text,
                        int gpu_rasterization_msaa_sample_count);

  void CompleteTasks(const Task::Vector& tasks);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  std::unique_ptr<GpuRasterizer> rasterizer_;

  Task::Vector completed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(GpuTileTaskWorkerPool);
};

}  // namespace cc

#endif  // CC_RASTER_GPU_TILE_TASK_WORKER_POOL_H_
