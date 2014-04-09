// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_DIRECT_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_DIRECT_RASTER_WORKER_POOL_H_

#include "base/memory/weak_ptr.h"
#include "cc/resources/raster_worker_pool.h"

namespace cc {
class ContextProvider;
class ResourceProvider;

class CC_EXPORT DirectRasterWorkerPool : public RasterWorkerPool,
                                         public internal::WorkerPoolTaskClient {
 public:
  virtual ~DirectRasterWorkerPool();

  static scoped_ptr<DirectRasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      ResourceProvider* resource_provider,
      ContextProvider* context_provider);

  // Overridden from RasterWorkerPool:
  virtual void SetClient(RasterWorkerPoolClient* client) OVERRIDE;
  virtual void Shutdown() OVERRIDE {}
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
  void OnRasterFinished();
  void OnRasterRequiredForActivationFinished();
  void ScheduleRunTasksOnOriginThread();
  void RunTasksOnOriginThread();
  void RunTaskOnOriginThread(internal::WorkerPoolTask* task);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  RasterWorkerPoolClient* client_;
  ResourceProvider* resource_provider_;
  ContextProvider* context_provider_;

  bool run_tasks_on_origin_thread_pending_;

  RasterTaskQueue raster_tasks_;

  bool raster_tasks_pending_;
  bool raster_tasks_required_for_activation_pending_;

  base::WeakPtrFactory<DirectRasterWorkerPool>
      raster_finished_weak_ptr_factory_;

  scoped_refptr<internal::WorkerPoolTask> raster_finished_task_;
  scoped_refptr<internal::WorkerPoolTask>
      raster_required_for_activation_finished_task_;

  internal::WorkerPoolTask::Vector completed_tasks_;

  base::WeakPtrFactory<DirectRasterWorkerPool> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DirectRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_DIRECT_RASTER_WORKER_POOL_H_
