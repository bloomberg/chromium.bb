// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_

#include <deque>

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT PixelBufferRasterWorkerPool : public RasterWorkerPool,
                                              public WorkerPoolClient {
 public:
  virtual ~PixelBufferRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      ResourceProvider* resource_provider, size_t num_threads) {
    return make_scoped_ptr<RasterWorkerPool>(
        new PixelBufferRasterWorkerPool(resource_provider, num_threads));
  }

  // Overridden from WorkerPool:
  virtual void Shutdown() OVERRIDE;
  virtual void CheckForCompletedTasks() OVERRIDE;

  // Overridden from RasterWorkerPool:
  virtual void ScheduleTasks(RasterTask::Queue* queue) OVERRIDE;
  virtual bool ForceUploadToComplete(const RasterTask& raster_task) OVERRIDE;

 private:
  PixelBufferRasterWorkerPool(ResourceProvider* resource_provider,
                              size_t num_threads);

  // Overridden from WorkerPoolClient:
  virtual void DidFinishDispatchingWorkerPoolCompletionCallbacks() OVERRIDE;

  bool CanScheduleRasterTask(internal::RasterWorkerPoolTask* task);
  void ScheduleMoreTasks();
  void OnRasterTaskCompleted(
      scoped_refptr<internal::RasterWorkerPoolTask> task,
      bool was_canceled,
      bool needs_upload);
  void AbortPendingUploads();
  void DidFinishRasterTask(internal::RasterWorkerPoolTask* task);

  TaskMap pixel_buffer_tasks_;

  typedef std::deque<scoped_refptr<internal::RasterWorkerPoolTask> > TaskDeque;
  TaskDeque tasks_with_pending_upload_;

  size_t bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;
  bool did_dispatch_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
