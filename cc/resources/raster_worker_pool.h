// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_H_

#include <vector>

#include "base/hash_tables.h"
#include "cc/base/worker_pool.h"
#include "cc/resources/resource_provider.h"

class SkDevice;

namespace cc {
class PicturePileImpl;
class PixelBufferRasterWorkerPool;
class Resource;

namespace internal {

class CC_EXPORT RasterWorkerPoolTask
    : public base::RefCounted<RasterWorkerPoolTask> {
 public:
  typedef std::vector<scoped_refptr<RasterWorkerPoolTask> > TaskVector;

  virtual bool RunOnThread(SkDevice* device, unsigned thread_index) = 0;
  virtual void DispatchCompletionCallback() = 0;

  void DidRun();
  bool HasFinishedRunning() const;
  void DidComplete();
  bool HasCompleted() const;

  const Resource* resource() const { return resource_; }
  const WorkerPoolTask::TaskVector& dependencies() const {
    return dependencies_;
  }

 protected:
  friend class base::RefCounted<RasterWorkerPoolTask>;

  RasterWorkerPoolTask(const Resource* resource,
                       WorkerPoolTask::TaskVector* dependencies);
  virtual ~RasterWorkerPoolTask();

 private:
  bool did_run_;
  bool did_complete_;
  const Resource* resource_;
  WorkerPoolTask::TaskVector dependencies_;
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

// A worker thread pool that runs raster tasks.
class CC_EXPORT RasterWorkerPool : public WorkerPool {
 public:
  class CC_EXPORT Task {
   public:
    class CC_EXPORT Set {
     public:
      typedef internal::WorkerPoolTask::TaskVector TaskVector;

      Set();
      ~Set();

      void Insert(const Task& task);

     private:
      friend class RasterWorkerPool;

      TaskVector tasks_;
    };

    Task();
    Task(const base::Closure& callback, const base::Closure& reply);
    ~Task();

    // Returns true if Task is null (doesn't refer to anything).
    bool is_null() const { return !internal_.get(); }

    // Returns the Task into an uninitialized state.
    void Reset();

   protected:
    friend class RasterWorkerPool;

    scoped_refptr<internal::WorkerPoolTask> internal_;
  };

  class CC_EXPORT RasterTask {
   public:
    // Returns true if |device| was written to. False indicate that
    // the content of |device| is undefined and the resource doesn't
    // need to be initialized.
    typedef base::Callback<bool(SkDevice* device,
                                PicturePileImpl* picture_pile)> Callback;

    typedef base::Callback<void(bool was_canceled)> Reply;

    class CC_EXPORT Queue {
     public:
      typedef internal::RasterWorkerPoolTask::TaskVector TaskVector;

      Queue();
      ~Queue();

      void Append(const RasterTask& task);

     private:
      friend class RasterWorkerPool;

      TaskVector tasks_;
    };

    RasterTask();
    RasterTask(PicturePileImpl* picture_pile,
               const Resource* resource,
               const Callback& callback,
               const Reply& reply,
               Task::Set* dependencies);
    ~RasterTask();

    // Returns true if Task is null (doesn't refer to anything).
    bool is_null() const { return !internal_; }

    // Returns the Task into an uninitialized state.
    void Reset();

   protected:
    friend class PixelBufferRasterWorkerPool;
    friend class RasterWorkerPool;

    scoped_refptr<internal::RasterWorkerPoolTask> internal_;
  };

  virtual ~RasterWorkerPool();

  // Tells the worker pool to shutdown after canceling all previously
  // scheduled tasks. Reply callbacks are still guaranteed to run.
  virtual void Shutdown() OVERRIDE;

  // Schedule running of raster tasks in |queue| and all dependencies.
  // Previously scheduled tasks that are no longer needed to run
  // raster tasks in |queue| will be canceled unless already running.
  // Once scheduled, reply callbacks are guaranteed to run for all tasks
  // even if they later get canceled by another call to ScheduleTasks().
  virtual void ScheduleTasks(RasterTask::Queue* queue) = 0;

  // Tells the raster worker pool to force any pending uploads for
  // |raster_task| to complete. Returns true when successful.
  virtual bool ForceUploadToComplete(const RasterTask& raster_task);

 protected:
  class RootTask {
   public:
    RootTask();
    explicit RootTask(internal::WorkerPoolTask::TaskVector* dependencies);
    RootTask(const base::Closure& callback,
             internal::WorkerPoolTask::TaskVector* dependencies);
    ~RootTask();

   protected:
    friend class RasterWorkerPool;

    scoped_refptr<internal::WorkerPoolTask> internal_;
  };

  typedef internal::RasterWorkerPoolTask* TaskMapKey;
  typedef base::hash_map<TaskMapKey,
                         scoped_refptr<internal::WorkerPoolTask> > TaskMap;

  RasterWorkerPool(ResourceProvider* resource_provider, size_t num_threads);

  void SetRasterTasks(RasterTask::Queue* queue);
  void ScheduleRasterTasks(const RootTask& root);

  ResourceProvider* resource_provider() const { return resource_provider_; }
  const RasterTask::Queue::TaskVector& raster_tasks() const {
    return raster_tasks_;
  }

 private:
  ResourceProvider* resource_provider_;
  RasterTask::Queue::TaskVector raster_tasks_;
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_H_
