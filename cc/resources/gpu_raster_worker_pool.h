// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_GPU_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_GPU_RASTER_WORKER_POOL_H_

#include "base/memory/weak_ptr.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/rasterizer.h"
#include "third_party/skia/include/core/SkMultiPictureDraw.h"

namespace cc {
class ContextProvider;
class ResourceProvider;

class CC_EXPORT GpuRasterWorkerPool : public RasterWorkerPool,
                                      public Rasterizer,
                                      public RasterizerTaskClient {
 public:
  virtual ~GpuRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      ContextProvider* context_provider,
      ResourceProvider* resource_provider);

  // Overridden from RasterWorkerPool:
  virtual Rasterizer* AsRasterizer() OVERRIDE;

  // Overridden from Rasterizer:
  virtual void SetClient(RasterizerClient* client) OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from RasterizerTaskClient:
  virtual scoped_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource) OVERRIDE;
  virtual void ReleaseBufferForRaster(scoped_ptr<RasterBuffer> buffer) OVERRIDE;

 private:
  GpuRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                      ContextProvider* context_provider,
                      ResourceProvider* resource_provider);

  void OnRasterFinished(TaskSet task_set);
  void ScheduleRunTasksOnOriginThread();
  void RunTasksOnOriginThread();
  void RunTaskOnOriginThread(RasterizerTask* task);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<TaskGraphRunner> task_graph_runner_;
  const NamespaceToken namespace_token_;
  RasterizerClient* client_;
  ContextProvider* context_provider_;
  ResourceProvider* resource_provider_;
  SkMultiPictureDraw multi_picture_draw_;

  bool run_tasks_on_origin_thread_pending_;

  TaskSetCollection raster_pending_;

  scoped_refptr<RasterizerTask> raster_finished_tasks_[kNumberOfTaskSets];

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  TaskGraph graph_;
  Task::Vector completed_tasks_;

  base::WeakPtrFactory<GpuRasterWorkerPool> raster_finished_weak_ptr_factory_;

  base::WeakPtrFactory<GpuRasterWorkerPool> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_GPU_RASTER_WORKER_POOL_H_
