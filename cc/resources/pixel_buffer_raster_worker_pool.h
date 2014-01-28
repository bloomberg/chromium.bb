// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_

#include <deque>

#include "base/cancelable_callback.h"
#include "base/containers/hash_tables.h"
#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT PixelBufferRasterWorkerPool
    : public RasterWorkerPool,
      public internal::WorkerPoolTaskClient {
 public:
  virtual ~PixelBufferRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      ResourceProvider* resource_provider,
      ContextProvider* context_provider,
      size_t max_transfer_buffer_usage_bytes) {
    return make_scoped_ptr<RasterWorkerPool>(new PixelBufferRasterWorkerPool(
        resource_provider, context_provider, max_transfer_buffer_usage_bytes));
  }

  // Overridden from RasterWorkerPool:
  virtual void Shutdown() OVERRIDE;
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
  enum RasterTaskState { UNSCHEDULED, SCHEDULED, UPLOADING, COMPLETED };
  typedef std::deque<scoped_refptr<internal::RasterWorkerPoolTask> >
      RasterTaskDeque;
  typedef internal::RasterWorkerPoolTask* RasterTaskMapKey;
  typedef base::hash_map<RasterTaskMapKey, RasterTaskState> RasterTaskStateMap;

  PixelBufferRasterWorkerPool(ResourceProvider* resource_provider,
                              ContextProvider* context_provider,
                              size_t max_transfer_buffer_usage_bytes);

  // Overridden from RasterWorkerPool:
  virtual void OnRasterTasksFinished() OVERRIDE;
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE;

  void FlushUploads();
  void CheckForCompletedUploads();
  void ScheduleCheckForCompletedRasterTasks();
  void CheckForCompletedRasterTasks();
  void ScheduleMoreTasks();
  unsigned PendingRasterTaskCount() const;
  bool HasPendingTasks() const;
  bool HasPendingTasksRequiredForActivation() const;
  void CheckForCompletedWorkerPoolTasks();

  const char* StateName() const;
  scoped_ptr<base::Value> StateAsValue() const;
  scoped_ptr<base::Value> ThrottleStateAsValue() const;

  bool shutdown_;

  RasterTaskSet raster_tasks_required_for_activation_;
  RasterTaskStateMap raster_task_states_;
  RasterTaskDeque raster_tasks_with_pending_upload_;
  RasterTaskDeque completed_raster_tasks_;
  TaskDeque completed_image_decode_tasks_;

  size_t scheduled_raster_task_count_;
  size_t bytes_pending_upload_;
  size_t max_bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;
  base::CancelableClosure check_for_completed_raster_tasks_callback_;
  bool check_for_completed_raster_tasks_pending_;

  bool should_notify_client_if_no_tasks_are_pending_;
  bool should_notify_client_if_no_tasks_required_for_activation_are_pending_;
  bool raster_finished_task_pending_;
  bool raster_required_for_activation_finished_task_pending_;
  ResourceFormat format_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
