// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT ImageRasterWorkerPool : public RasterWorkerPool {
 public:
  virtual ~ImageRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      ResourceProvider* resource_provider,
      unsigned texture_target);

  // Overridden from RasterWorkerPool:
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE;
  virtual unsigned GetResourceTarget() const OVERRIDE;
  virtual ResourceFormat GetResourceFormat() const OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from internal::WorkerPoolTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(internal::RasterWorkerPoolTask* task)
      OVERRIDE;
  virtual void OnRasterCompleted(internal::RasterWorkerPoolTask* task,
                                 const PicturePileImpl::Analysis& analysis)
      OVERRIDE;
  virtual void OnImageDecodeCompleted(internal::WorkerPoolTask* task) OVERRIDE {
  }

 protected:
  ImageRasterWorkerPool(internal::TaskGraphRunner* task_graph_runner,
                        ResourceProvider* resource_provider,
                        unsigned texture_target);

 private:
  // Overridden from RasterWorkerPool:
  virtual void OnRasterTasksFinished() OVERRIDE;
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE;

  scoped_ptr<base::Value> StateAsValue() const;

  const unsigned texture_target_;

  RasterTaskQueue raster_tasks_;

  bool raster_tasks_pending_;
  bool raster_tasks_required_for_activation_pending_;

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  internal::TaskGraph graph_;
  internal::Task::Vector completed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(ImageRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
