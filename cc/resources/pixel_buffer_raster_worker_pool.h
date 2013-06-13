// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_

#include <deque>
#include <set>

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT PixelBufferRasterWorkerPool : public RasterWorkerPool {
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

  void FlushUploads();
  void CheckForCompletedUploads();
  void ScheduleCheckForCompletedRasterTasks();
  void CheckForCompletedRasterTasks();
  void ScheduleMoreTasks();
  void OnRasterTaskCompleted(
      scoped_refptr<internal::RasterWorkerPoolTask> task,
      bool was_canceled,
      bool needs_upload);
  void AbortPendingUploads();
  void DidCompleteRasterTask(internal::RasterWorkerPoolTask* task);
  void OnRasterFinished(int64 schedule_more_tasks_count);

  static void RunRasterFinishedTask(
      scoped_refptr<base::MessageLoopProxy> origin_loop,
      const base::Closure& on_raster_finished_callback);

  TaskMap pixel_buffer_tasks_;

  typedef std::deque<scoped_refptr<internal::RasterWorkerPoolTask> > TaskDeque;
  TaskDeque tasks_with_pending_upload_;
  TaskDeque completed_tasks_;

  size_t bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;
  base::CancelableClosure check_for_completed_raster_tasks_callback_;
  bool check_for_completed_raster_tasks_pending_;

  base::WeakPtrFactory<PixelBufferRasterWorkerPool> weak_ptr_factory_;
  int64 schedule_more_tasks_count_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
