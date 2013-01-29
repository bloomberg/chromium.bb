// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_WORKER_POOL_H_
#define CC_RASTER_WORKER_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread.h"
#include "cc/rendering_stats.h"
#include "ui/gfx/rect.h"

namespace skia {
class LazyPixelRef;
}

namespace cc {
class PicturePileImpl;

class RasterWorkerPool {
 public:
  explicit RasterWorkerPool(size_t num_raster_threads);
  virtual ~RasterWorkerPool();

  static scoped_ptr<RasterWorkerPool> Create(size_t num_raster_threads) {
    return make_scoped_ptr(new RasterWorkerPool(num_raster_threads));
  }

  // Starts the worker pool. Returns true if the worker pool was successfully
  // started; otherwise, returns false.
  bool Start();

  // Tells the worker pool to stop and returns once all pending tasks have
  // completed.
  void Stop();

  // Returns true if the worker pool has been started, and not yet stopped.
  bool IsRunning() const;

  bool IsBusy();

  void PostRasterTaskAndReply(PicturePileImpl* picture_pile,
                              uint8* buffer,
                              const gfx::Rect& rect,
                              float contents_scale,
                              const base::Closure& reply);
  void PostImageDecodeTaskAndReply(skia::LazyPixelRef* pixel_ref,
                                   const base::Closure& reply);

  void GetRenderingStats(RenderingStats* stats);

 private:
  class Thread : public base::Thread {
   public:
    class Task {
     public:
      Task(Thread* thread);
      ~Task();

     private:
      friend class RasterWorkerPool;

      Thread* thread_;
      RenderingStats rendering_stats_;

      DISALLOW_COPY_AND_ASSIGN(Task);
    };

    virtual void Init() OVERRIDE;

    Thread(const std::string name);
    virtual ~Thread();

    int num_pending_tasks() const { return num_pending_tasks_; }
    RenderingStats& rendering_stats() { return rendering_stats_; }

   private:
    int num_pending_tasks_;
    RenderingStats rendering_stats_;

    DISALLOW_COPY_AND_ASSIGN(Thread);
  };

  class PendingTaskComparator {
   public:
    bool operator() (const Thread* a, const Thread* b) const {
      return a->num_pending_tasks() < b->num_pending_tasks();
    }
  };

  Thread::Task* CreateTask();
  void DestroyTask(Thread::Task* task);

  void OnTaskCompleted(Thread::Task* task,
                       const base::Closure& reply);
  void OnRasterTaskCompleted(Thread::Task* task,
                             scoped_refptr<PicturePileImpl> picture_pile,
                             const base::Closure& reply);

  void SortRasterThreadsIfNeeded();

  bool is_running_;

  typedef std::vector<Thread*> ThreadVector;
  ThreadVector raster_threads_;
  bool raster_threads_need_sorting_;

  DISALLOW_COPY_AND_ASSIGN(RasterWorkerPool);
};

}  // namespace cc

#endif  // CC_RASTER_WORKER_POOL_H_
