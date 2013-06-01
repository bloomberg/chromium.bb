// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RASTER_WORKER_POOL_H_
#define CC_RESOURCES_RASTER_WORKER_POOL_H_

#include "cc/base/worker_pool.h"

namespace cc {
class PicturePileImpl;

// A worker thread pool that runs raster tasks.
class CC_EXPORT RasterWorkerPool : public WorkerPool {
 public:
  class Task {
   public:
    typedef base::Callback<void(bool)> Reply;

    // Highest priority task first. Order of execution is not guaranteed.
    class Queue {
     public:
      Queue();
      ~Queue();

      bool empty() const { return tasks_.empty(); }

      // Add task to the back of the queue.
      void Append(const Task& task);

     private:
      friend class RasterWorkerPool;

      internal::WorkerPoolTask::TaskVector tasks_;
    };

    Task();
    Task(const base::Closure& callback, const Reply& reply);
    explicit Task(Queue* dependencies);
    ~Task();

    // Returns true if Task is null (doesn't refer to anything).
    bool is_null() const { return !internal_.get(); }

    // Returns the Task into an uninitialized state.
    void Reset();

   protected:
    friend class RasterWorkerPool;

    explicit Task(scoped_refptr<internal::WorkerPoolTask> internal);

    scoped_refptr<internal::WorkerPoolTask> internal_;
  };

  class PictureTask : public Task {
   public:
    typedef base::Callback<void(PicturePileImpl*)> Callback;

    PictureTask(PicturePileImpl* picture_pile,
                const Callback& callback,
                const Reply& reply,
                Queue* dependencies);
  };

  virtual ~RasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(size_t num_threads) {
    return make_scoped_ptr(new RasterWorkerPool(num_threads));
  }

  // Tells the worker pool to shutdown after canceling all previously
  // scheduled tasks. Reply callbacks are still guaranteed to run.
  virtual void Shutdown() OVERRIDE;

  // Schedule running of |root| task and all its dependencies. Tasks
  // previously scheduled but no longer needed to run |root| will be
  // canceled unless already running. Once scheduled, reply callbacks
  // are guaranteed to run for all tasks even if they later get
  // canceled by another call to ScheduleTasks().
  void ScheduleTasks(Task* root);

 private:
  explicit RasterWorkerPool(size_t num_threads);

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RASTER_WORKER_POOL_H_
