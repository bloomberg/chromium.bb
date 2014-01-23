// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_H_

#include <deque>
#include <vector>

#include "base/containers/hash_tables.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/raster_mode.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/task_graph_runner.h"
#include "cc/resources/tile_priority.h"

class SkPixelRef;

namespace cc {

class ContextProvider;
class Resource;
class ResourceProvider;

namespace internal {

class CC_EXPORT WorkerPoolTask : public Task {
 public:
  virtual void CompleteOnOriginThread() = 0;

  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

 protected:
  WorkerPoolTask();
  virtual ~WorkerPoolTask();

 private:
  bool did_complete_;
};

class CC_EXPORT RasterWorkerPoolTask
    : public base::RefCounted<RasterWorkerPoolTask> {
 public:
  // Returns true if |buffer| was written to. False indicate that
  // the content of |buffer| is undefined and the resource doesn't
  // need to be initialized.
  virtual bool RunOnWorkerThread(unsigned thread_index,
                                 void* buffer,
                                 gfx::Size size,
                                 int stride) = 0;
  virtual void RunOnOriginThread(ResourceProvider* resource_provider,
                                 ContextProvider* context_provider) = 0;
  virtual void CompleteOnOriginThread() = 0;

  void DidRun(bool was_canceled);
  bool HasFinishedRunning() const;
  bool WasCanceled() const;
  void WillComplete();
  void DidComplete();
  bool HasCompleted() const;

  const Resource* resource() const { return resource_; }
  const internal::Task::Vector& dependencies() const { return dependencies_; }
  bool use_gpu_rasterization() const { return use_gpu_rasterization_; }

 protected:
  friend class base::RefCounted<RasterWorkerPoolTask>;

  RasterWorkerPoolTask(const Resource* resource,
                       internal::Task::Vector* dependencies,
                       bool use_gpu_rasterization);
  virtual ~RasterWorkerPoolTask();

 private:
  bool did_run_;
  bool did_complete_;
  bool was_canceled_;
  const Resource* resource_;
  Task::Vector dependencies_;
  bool use_gpu_rasterization_;
};

}  // namespace internal
}  // namespace cc

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <> struct hash<cc::internal::RasterWorkerPoolTask*> {
  size_t operator()(cc::internal::RasterWorkerPoolTask* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace cc {

class CC_EXPORT RasterWorkerPoolClient {
 public:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const = 0;
  virtual void DidFinishRunningTasks() = 0;
  virtual void DidFinishRunningTasksRequiredForActivation() = 0;

 protected:
  virtual ~RasterWorkerPoolClient() {}
};

// A worker thread pool that runs raster tasks.
class CC_EXPORT RasterWorkerPool {
 public:
  class CC_EXPORT Task {
   public:
    typedef base::Callback<void(bool was_canceled)> Reply;

    class CC_EXPORT Set {
     public:
      Set();
      ~Set();

      void Insert(const Task& task);

     private:
      friend class RasterWorkerPool;
      friend class RasterWorkerPoolTest;

      internal::Task::Vector tasks_;
    };

    Task();
    ~Task();

    // Returns true if Task is null (doesn't refer to anything).
    bool is_null() const { return !internal_.get(); }

    // Returns the Task into an uninitialized state.
    void Reset();

   protected:
    friend class RasterWorkerPool;
    friend class RasterWorkerPoolTest;

    explicit Task(internal::WorkerPoolTask* internal);

    scoped_refptr<internal::WorkerPoolTask> internal_;
  };

  class CC_EXPORT RasterTask {
   public:
    typedef base::Callback<void(const PicturePileImpl::Analysis& analysis,
                                bool was_canceled)> Reply;

    class CC_EXPORT Queue {
     public:
      Queue();
      ~Queue();

      void Append(const RasterTask& task, bool required_for_activation);

     private:
      friend class RasterWorkerPool;

      typedef std::vector<scoped_refptr<internal::RasterWorkerPoolTask> >
          TaskVector;
      TaskVector tasks_;
      typedef base::hash_set<internal::RasterWorkerPoolTask*> TaskSet;
      TaskSet tasks_required_for_activation_;
    };

    RasterTask();
    ~RasterTask();

    // Returns true if Task is null (doesn't refer to anything).
    bool is_null() const { return !internal_.get(); }

    // Returns the Task into an uninitialized state.
    void Reset();

   protected:
    friend class RasterWorkerPool;
    friend class RasterWorkerPoolTest;

    explicit RasterTask(internal::RasterWorkerPoolTask* internal);

    scoped_refptr<internal::RasterWorkerPoolTask> internal_;
  };

  virtual ~RasterWorkerPool();

  static void SetNumRasterThreads(int num_threads);

  static int GetNumRasterThreads();

  // TODO(vmpstr): Figure out an elegant way to not pass this many parameters.
  static RasterTask CreateRasterTask(
      const Resource* resource,
      PicturePileImpl* picture_pile,
      const gfx::Rect& content_rect,
      float contents_scale,
      RasterMode raster_mode,
      TileResolution tile_resolution,
      int layer_id,
      const void* tile_id,
      int source_frame_number,
      bool use_gpu_rasterization,
      RenderingStatsInstrumentation* rendering_stats,
      const RasterTask::Reply& reply,
      Task::Set* dependencies);

  static Task CreateImageDecodeTask(
      SkPixelRef* pixel_ref,
      int layer_id,
      RenderingStatsInstrumentation* stats_instrumentation,
      const Task::Reply& reply);

  void SetClient(RasterWorkerPoolClient* client);

  // Tells the worker pool to shutdown after canceling all previously
  // scheduled tasks. Reply callbacks are still guaranteed to run.
  virtual void Shutdown();

  // Schedule running of raster tasks in |queue| and all dependencies.
  // Previously scheduled tasks that are no longer needed to run
  // raster tasks in |queue| will be canceled unless already running.
  // Once scheduled, reply callbacks are guaranteed to run for all tasks
  // even if they later get canceled by another call to ScheduleTasks().
  virtual void ScheduleTasks(RasterTask::Queue* queue) = 0;

  // Force a check for completed tasks.
  virtual void CheckForCompletedTasks();

  // Returns the target that needs to be used for raster task resources.
  virtual unsigned GetResourceTarget() const = 0;

  // Returns the format that needs to be used for raster task resources.
  virtual ResourceFormat GetResourceFormat() const = 0;

 protected:
  typedef internal::TaskGraphRunner::TaskGraph TaskGraph;
  typedef std::vector<scoped_refptr<internal::WorkerPoolTask> > TaskVector;
  typedef std::vector<scoped_refptr<internal::RasterWorkerPoolTask> >
      RasterTaskVector;
  typedef std::deque<scoped_refptr<internal::RasterWorkerPoolTask> >
      RasterTaskDeque;
  typedef base::hash_set<internal::RasterWorkerPoolTask*> RasterTaskSet;
  typedef internal::RasterWorkerPoolTask* TaskMapKey;
  typedef base::hash_map<TaskMapKey, scoped_refptr<internal::WorkerPoolTask> >
      TaskMap;

  RasterWorkerPool(ResourceProvider* resource_provider,
                   ContextProvider* context_provider);

  virtual void OnRasterTasksFinished() = 0;
  virtual void OnRasterTasksRequiredForActivationFinished() = 0;

  void CheckForCompletedWorkerPoolTasks();
  void SetTaskGraph(TaskGraph* graph);

  void SetRasterTasks(RasterTask::Queue* queue);
  bool IsRasterTaskRequiredForActivation(
      internal::RasterWorkerPoolTask* task) const;
  // Run raster tasks that use GPU on current thread.
  void RunGpuRasterTasks(const RasterTaskVector& tasks);

  RasterWorkerPoolClient* client() const { return client_; }
  ResourceProvider* resource_provider() const { return resource_provider_; }
  ContextProvider* context_provider() const { return context_provider_; }
  const RasterTaskVector& raster_tasks() const { return raster_tasks_; }
  const RasterTaskSet& raster_tasks_required_for_activation() const {
    return raster_tasks_required_for_activation_;
  }
  void set_raster_finished_task(
      internal::WorkerPoolTask* raster_finished_task) {
    raster_finished_task_ = raster_finished_task;
  }
  void set_raster_required_for_activation_finished_task(
      internal::WorkerPoolTask* raster_required_for_activation_finished_task) {
    raster_required_for_activation_finished_task_ =
        raster_required_for_activation_finished_task;
  }

  scoped_refptr<internal::WorkerPoolTask> CreateRasterFinishedTask();
  scoped_refptr<internal::WorkerPoolTask>
      CreateRasterRequiredForActivationFinishedTask(
          size_t tasks_required_for_activation_count);

  scoped_ptr<base::Value> ScheduledStateAsValue() const;

  static internal::GraphNode* CreateGraphNodeForTask(
      internal::WorkerPoolTask* task,
      unsigned priority,
      TaskGraph* graph);

  static internal::GraphNode* CreateGraphNodeForRasterTask(
      internal::WorkerPoolTask* raster_task,
      const internal::Task::Vector& decode_tasks,
      unsigned priority,
      TaskGraph* graph);

 private:
  void OnRasterFinished(const internal::WorkerPoolTask* source);
  void OnRasterRequiredForActivationFinished(
      const internal::WorkerPoolTask* source);

  internal::NamespaceToken namespace_token_;
  RasterWorkerPoolClient* client_;
  ResourceProvider* resource_provider_;
  ContextProvider* context_provider_;
  RasterTask::Queue::TaskVector raster_tasks_;
  RasterTask::Queue::TaskSet raster_tasks_required_for_activation_;
  RasterTaskDeque completed_gpu_raster_tasks_;

  scoped_refptr<internal::WorkerPoolTask> raster_finished_task_;
  scoped_refptr<internal::WorkerPoolTask>
      raster_required_for_activation_finished_task_;
  base::WeakPtrFactory<RasterWorkerPool> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_H_
