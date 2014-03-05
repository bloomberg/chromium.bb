// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DIRECT_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_DIRECT_RASTER_WORKER_POOL_H_

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class ContextProvider;

class CC_EXPORT DirectRasterWorkerPool : public RasterWorkerPool {
 public:
  virtual ~DirectRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      ResourceProvider* resource_provider,
      ContextProvider* context_provider);

  // Overridden from RasterWorkerPool:
  virtual void ScheduleTasks(RasterTaskQueue* queue) OVERRIDE;
  virtual unsigned GetResourceTarget() const OVERRIDE;
  virtual ResourceFormat GetResourceFormat() const OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from internal::WorkerPoolTaskClient:
  virtual SkCanvas* AcquireCanvasForRaster(internal::WorkerPoolTask* task,
                                           const Resource* resource) OVERRIDE;
  virtual void ReleaseCanvasForRaster(internal::WorkerPoolTask* task,
                                      const Resource* resource) OVERRIDE;

 protected:
  DirectRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                         ResourceProvider* resource_provider,
                         ContextProvider* context_provider);

 private:
  // Overridden from RasterWorkerPool:
  virtual void OnRasterTasksFinished() OVERRIDE;
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE;

  void ScheduleRunTasksOnOriginThread();
  void RunTasksOnOriginThread();

  ContextProvider* context_provider_;

  bool run_tasks_on_origin_thread_pending_;

  RasterTaskQueue raster_tasks_;

  bool raster_tasks_pending_;
  bool raster_tasks_required_for_activation_pending_;

  internal::WorkerPoolTask::Vector completed_tasks_;

  base::WeakPtrFactory<DirectRasterWorkerPool> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DirectRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_DIRECT_RASTER_WORKER_POOL_H_
