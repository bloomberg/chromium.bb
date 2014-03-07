// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_

#include <vector>

#include "cc/resources/raster_worker_pool.h"

namespace cc {

class CC_EXPORT PixelBufferRasterWorkerPool : public RasterWorkerPool {
 public:
  virtual ~PixelBufferRasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(
      base::SequencedTaskRunner* task_runner,
      ResourceProvider* resource_provider,
      size_t max_transfer_buffer_usage_bytes);

  // Overridden from RasterWorkerPool:
  virtual void Shutdown() OVERRIDE;
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
  PixelBufferRasterWorkerPool(base::SequencedTaskRunner* task_runner,
                              internal::TaskGraphRunner* task_graph_runner,
                              ResourceProvider* resource_provider,
                              size_t max_transfer_buffer_usage_bytes);

 private:
  struct RasterTaskState {
    class TaskComparator {
     public:
      explicit TaskComparator(const internal::WorkerPoolTask* task)
          : task_(task) {}

      bool operator()(const RasterTaskState& state) const {
        return state.task == task_;
      }

     private:
      const internal::WorkerPoolTask* task_;
    };

    typedef std::vector<RasterTaskState> Vector;

    RasterTaskState(internal::WorkerPoolTask* task,
                    bool required_for_activation)
        : type(UNSCHEDULED),
          task(task),
          resource(NULL),
          required_for_activation(required_for_activation) {}

    enum { UNSCHEDULED, SCHEDULED, UPLOADING, COMPLETED } type;
    internal::WorkerPoolTask* task;
    const Resource* resource;
    bool required_for_activation;
  };

  // Overridden from RasterWorkerPool:
  virtual void OnRasterTasksFinished() OVERRIDE;
  virtual void OnRasterTasksRequiredForActivationFinished() OVERRIDE;

  void FlushUploads();
  void CheckForCompletedUploads();
  void ScheduleCheckForCompletedRasterTasks();
  void OnCheckForCompletedRasterTasks();
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

  RasterTaskQueue raster_tasks_;
  RasterTaskState::Vector raster_task_states_;
  TaskDeque raster_tasks_with_pending_upload_;
  TaskVector completed_raster_tasks_;
  TaskVector completed_image_decode_tasks_;

  size_t scheduled_raster_task_count_;
  size_t raster_tasks_required_for_activation_count_;
  size_t bytes_pending_upload_;
  size_t max_bytes_pending_upload_;
  bool has_performed_uploads_since_last_flush_;
  base::TimeTicks check_for_completed_raster_tasks_time_;
  bool check_for_completed_raster_tasks_pending_;

  bool should_notify_client_if_no_tasks_are_pending_;
  bool should_notify_client_if_no_tasks_required_for_activation_are_pending_;
  bool raster_finished_task_pending_;
  bool raster_required_for_activation_finished_task_pending_;

  // Task graph used when scheduling tasks and vector used to gather
  // completed tasks.
  internal::TaskGraph graph_;
  internal::Task::Vector completed_tasks_;

  base::WeakPtrFactory<PixelBufferRasterWorkerPool> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PixelBufferRasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_PIXEL_BUFFER_RASTER_WORKER_POOL_H_
