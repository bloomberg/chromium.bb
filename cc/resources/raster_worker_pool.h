// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_H_

#include <deque>
#include <vector>

#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/raster_mode.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/task_graph_runner.h"
#include "cc/resources/tile_priority.h"

class SkPixelRef;

namespace cc {

class Resource;
class ResourceProvider;

namespace internal {

class WorkerPoolTask;

class CC_EXPORT WorkerPoolTaskClient {
 public:
  virtual SkCanvas* AcquireCanvasForRaster(WorkerPoolTask* task,
                                           const Resource* resource) = 0;
  virtual void ReleaseCanvasForRaster(WorkerPoolTask* task,
                                      const Resource* resource) = 0;

 protected:
  virtual ~WorkerPoolTaskClient() {}
};

class CC_EXPORT WorkerPoolTask : public Task {
 public:
  typedef std::vector<scoped_refptr<WorkerPoolTask> > Vector;

  virtual void ScheduleOnOriginThread(WorkerPoolTaskClient* client) = 0;
  virtual void RunOnOriginThread() = 0;
  virtual void CompleteOnOriginThread(WorkerPoolTaskClient* client) = 0;
  virtual void RunReplyOnOriginThread() = 0;

  void WillSchedule();
  void DidSchedule();
  bool HasBeenScheduled() const;

  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

 protected:
  WorkerPoolTask();
  virtual ~WorkerPoolTask();

  bool did_schedule_;
  bool did_complete_;
};

class CC_EXPORT RasterWorkerPoolTask : public WorkerPoolTask {
 public:
  const Resource* resource() const { return resource_; }
  const internal::WorkerPoolTask::Vector& dependencies() const {
    return dependencies_;
  }

 protected:
  RasterWorkerPoolTask(const Resource* resource,
                       internal::WorkerPoolTask::Vector* dependencies);
  virtual ~RasterWorkerPoolTask();

 private:
  const Resource* resource_;
  WorkerPoolTask::Vector dependencies_;
};

}  // namespace internal

class CC_EXPORT RasterWorkerPoolClient {
 public:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const = 0;
  virtual void DidFinishRunningTasks() = 0;
  virtual void DidFinishRunningTasksRequiredForActivation() = 0;

 protected:
  virtual ~RasterWorkerPoolClient() {}
};

struct CC_EXPORT RasterTaskQueue {
  struct CC_EXPORT Item {
    class TaskComparator {
     public:
      explicit TaskComparator(const internal::WorkerPoolTask* task)
          : task_(task) {}

      bool operator()(const Item& item) const { return item.task == task_; }

     private:
      const internal::WorkerPoolTask* task_;
    };

    typedef std::vector<Item> Vector;

    Item(internal::RasterWorkerPoolTask* task, bool required_for_activation);
    ~Item();

    static bool IsRequiredForActivation(const Item& item) {
      return item.required_for_activation;
    }

    internal::RasterWorkerPoolTask* task;
    bool required_for_activation;
  };

  RasterTaskQueue();
  ~RasterTaskQueue();

  void Swap(RasterTaskQueue* other);
  void Reset();

  Item::Vector items;
  size_t required_for_activation_count;
};

// A worker thread pool that runs raster tasks.
class CC_EXPORT RasterWorkerPool : public internal::WorkerPoolTaskClient {
 public:
  virtual ~RasterWorkerPool();

  static void SetNumRasterThreads(int num_threads);
  static int GetNumRasterThreads();

  static internal::TaskGraphRunner* GetTaskGraphRunner();

  static unsigned kOnDemandRasterTaskPriority;
  static unsigned kRasterFinishedTaskPriority;
  static unsigned kRasterRequiredForActivationFinishedTaskPriority;
  static unsigned kRasterTaskPriorityBase;

  // TODO(vmpstr): Figure out an elegant way to not pass this many parameters.
  static scoped_refptr<internal::RasterWorkerPoolTask> CreateRasterTask(
      const Resource* resource,
      PicturePileImpl* picture_pile,
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterMode raster_mode,
      TileResolution tile_resolution,
      int layer_id,
      const void* tile_id,
      int source_frame_number,
      RenderingStatsInstrumentation* rendering_stats,
      const base::Callback<void(const PicturePileImpl::Analysis&, bool)>& reply,
      internal::WorkerPoolTask::Vector* dependencies);

  static scoped_refptr<internal::WorkerPoolTask> CreateImageDecodeTask(
      SkPixelRef* pixel_ref,
      int layer_id,
      RenderingStatsInstrumentation* rendering_stats,
      const base::Callback<void(bool was_canceled)>& reply);

  void SetClient(RasterWorkerPoolClient* client);

  // Tells the worker pool to shutdown after canceling all previously
  // scheduled tasks. Reply callbacks are still guaranteed to run.
  virtual void Shutdown();

  // Schedule running of raster tasks in |queue| and all dependencies.
  // Previously scheduled tasks that are no longer needed to run
  // raster tasks in |queue| will be canceled unless already running.
  // Once scheduled, reply callbacks are guaranteed to run for all tasks
  // even if they later get canceled by another call to ScheduleTasks().
  virtual void ScheduleTasks(RasterTaskQueue* queue) = 0;

  // Force a check for completed tasks.
  virtual void CheckForCompletedTasks() = 0;

  // Returns the target that needs to be used for raster task resources.
  virtual unsigned GetResourceTarget() const = 0;

  // Returns the format that needs to be used for raster task resources.
  virtual ResourceFormat GetResourceFormat() const = 0;

 protected:
  typedef std::vector<scoped_refptr<internal::WorkerPoolTask> > TaskVector;
  typedef std::deque<scoped_refptr<internal::WorkerPoolTask> > TaskDeque;
  typedef std::vector<scoped_refptr<internal::RasterWorkerPoolTask> >
      RasterTaskVector;

  RasterWorkerPool(internal::TaskGraphRunner* task_graph_runner,
                   ResourceProvider* resource_provider);

  virtual void OnRasterTasksFinished() = 0;
  virtual void OnRasterTasksRequiredForActivationFinished() = 0;

  void SetTaskGraph(internal::TaskGraph* graph);
  void CollectCompletedWorkerPoolTasks(internal::Task::Vector* completed_tasks);

  RasterWorkerPoolClient* client() const { return client_; }
  ResourceProvider* resource_provider() const { return resource_provider_; }

  void set_raster_finished_task(
      internal::WorkerPoolTask* raster_finished_task) {
    raster_finished_task_ = raster_finished_task;
  }
  internal::WorkerPoolTask* raster_finished_task() const {
    return raster_finished_task_.get();
  }
  void set_raster_required_for_activation_finished_task(
      internal::WorkerPoolTask* raster_required_for_activation_finished_task) {
    raster_required_for_activation_finished_task_ =
        raster_required_for_activation_finished_task;
  }
  internal::WorkerPoolTask* raster_required_for_activation_finished_task()
      const {
    return raster_required_for_activation_finished_task_.get();
  }

  scoped_refptr<internal::WorkerPoolTask> CreateRasterFinishedTask();
  scoped_refptr<internal::WorkerPoolTask>
      CreateRasterRequiredForActivationFinishedTask(
          size_t tasks_required_for_activation_count);

  void RunTaskOnOriginThread(internal::WorkerPoolTask* task);

  static void InsertNodeForTask(internal::TaskGraph* graph,
                                internal::WorkerPoolTask* task,
                                unsigned priority,
                                size_t dependencies);

  static void InsertNodeForRasterTask(
      internal::TaskGraph* graph,
      internal::WorkerPoolTask* task,
      const internal::WorkerPoolTask::Vector& decode_tasks,
      unsigned priority);

 private:
  void OnRasterFinished(const internal::WorkerPoolTask* source);
  void OnRasterRequiredForActivationFinished(
      const internal::WorkerPoolTask* source);

  internal::TaskGraphRunner* task_graph_runner_;
  internal::NamespaceToken namespace_token_;
  RasterWorkerPoolClient* client_;
  ResourceProvider* resource_provider_;

  scoped_refptr<internal::WorkerPoolTask> raster_finished_task_;
  scoped_refptr<internal::WorkerPoolTask>
      raster_required_for_activation_finished_task_;
  base::WeakPtrFactory<RasterWorkerPool> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_H_
