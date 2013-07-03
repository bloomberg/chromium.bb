// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_

#include <deque>
#include <set>
#include <vector>

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
  virtual void OnRasterTasksFinished() OVERRIDE;
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE;

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
  void DidCompleteRasterTask(internal::RasterWorkerPoolTask* task);
  unsigned PendingRasterTaskCount() const;
  bool HasPendingTasks() const;
  bool HasPendingTasksRequiredForActivation() const;

  const char* StateName() const;
  scoped_ptr<base::Value> StateAsValue() const;
  scoped_ptr<base::Value> ThrottleStateAsValue() const;

  bool shutdown_;

  TaskMap pixel_buffer_tasks_;

  typedef std::deque<scoped_refptr<internal::RasterWorkerPoolTask> > TaskDeque;
  TaskDeque tasks_with_pending_upload_;
  TaskDeque completed_tasks_;

  typedef std::set<internal::RasterWorkerPoolTask*> TaskSet;
  TaskSet tasks_required_for_activation_;

  size_t scheduled_raster_task_count_;
  size_t bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;
  base::CancelableClosure check_for_completed_raster_tasks_callback_;
  bool check_for_completed_raster_tasks_pending_;

  bool should_notify_client_if_no_tasks_are_pending_;
  bool should_notify_client_if_no_tasks_required_for_activation_are_pending_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
