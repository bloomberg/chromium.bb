// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT ImageRasterWorkerPool : public RasterWorkerPool,
                                        public internal::WorkerPoolTaskClient {
 public:
  virtual ~ImageRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      ResourceProvider* resource_provider,
      ContextProvider* context_provider,
      unsigned texture_target) {
    return make_scoped_ptr<RasterWorkerPool>(new ImageRasterWorkerPool(
        resource_provider, context_provider, texture_target));
  }

  // Overridden from RasterWorkerPool:
  virtual void ScheduleTasks(RasterTask::Queue* queue) OVERRIDE;
  virtual unsigned GetResourceTarget() const OVERRIDE;
  virtual ResourceFormat GetResourceFormat() const OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from internal::WorkerPoolTaskClient:
  virtual void* AcquireBufferForRaster(internal::RasterWorkerPoolTask* task,
                                       int* stride) OVERRIDE;
  virtual void OnRasterCompleted(internal::RasterWorkerPoolTask* task,
                                 const PicturePileImpl::Analysis& analysis)
      OVERRIDE;
  virtual void OnImageDecodeCompleted(internal::WorkerPoolTask* task) OVERRIDE;

 private:
  ImageRasterWorkerPool(ResourceProvider* resource_provider,
                        ContextProvider* context_provider,
                        unsigned texture_target);

  // Overridden from RasterWorkerPool:
  virtual void OnRasterTasksFinished() OVERRIDE;
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE;

  scoped_ptr<base::Value> StateAsValue() const;

  static void CreateGraphNodeForImageRasterTask(
      internal::WorkerPoolTask* image_task,
      const internal::Task::Vector& decode_tasks,
      unsigned priority,
      bool is_required_for_activation,
      internal::GraphNode* raster_required_for_activation_finished_node,
      internal::GraphNode* raster_finished_node,
      TaskGraph* graph);

  const unsigned texture_target_;

  bool raster_tasks_pending_;
  bool raster_tasks_required_for_activation_pending_;

  DISALLOW_COPY_AND_ASSIGN(ImageRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_IMAGE_RASTER_WORKER_POOL_H_
