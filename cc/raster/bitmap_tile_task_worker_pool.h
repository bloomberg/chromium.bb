// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_BITMAP_TILE_TASK_WORKER_POOL_H_
#define CC_RASTER_BITMAP_TILE_TASK_WORKER_POOL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
#include "cc/raster/tile_task_runner.h"
#include "cc/raster/tile_task_worker_pool.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {
class ResourceProvider;

class CC_EXPORT BitmapTileTaskWorkerPool : public TileTaskWorkerPool,
                                           public TileTaskRunner,
                                           public TileTaskClient {
 public:
  ~BitmapTileTaskWorkerPool() override;

  static scoped_ptr<TileTaskWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      TaskGraphRunner* task_graph_runner,
      ResourceProvider* resource_provider);

  // Overridden from TileTaskWorkerPool:
  TileTaskRunner* AsTileTaskRunner() override;

  // Overridden from TileTaskRunner:
  void Shutdown() override;
  void ScheduleTasks(TaskGraph* graph) override;
  void CheckForCompletedTasks() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;

  // Overridden from TileTaskClient:
  scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) override;

 protected:
  BitmapTileTaskWorkerPool(base::SequencedTaskRunner* task_runner,
                           TaskGraphRunner* task_graph_runner,
                           ResourceProvider* resource_provider);

 private:
  scoped_refptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  TaskGraphRunner* task_graph_runner_;
  const NamespaceToken namespace_token_;
  ResourceProvider* resource_provider_;

  Task::Vector completed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(BitmapTileTaskWorkerPool);
};

}  // namespace cc

#endif  // CC_RASTER_BITMAP_TILE_TASK_WORKER_POOL_H_
