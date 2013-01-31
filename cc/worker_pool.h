// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_WORKER_POOL_H_
#define CC_WORKER_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "cc/rendering_stats.h"
#include "cc/scoped_ptr_deque.h"

namespace cc {
namespace internal {

class WorkerPoolTask {
 public:
  virtual ~WorkerPoolTask();

  virtual void Run() = 0;

  void Completed();

  RenderingStats& rendering_stats() { return rendering_stats_; }

 protected:
  WorkerPoolTask(const base::Closure& reply);

  base::Closure reply_;
  RenderingStats rendering_stats_;
};

}  // namespace internal

// A worker thread pool that runs rendering tasks and guarantees completion
// of all pending tasks at shutdown.
class WorkerPool {
 public:
  typedef base::Callback<void(RenderingStats*)> Callback;

  virtual ~WorkerPool();

  static scoped_ptr<WorkerPool> Create(size_t num_threads) {
    return make_scoped_ptr(new WorkerPool(num_threads));
  }

  // Tells the worker pool to shutdown and returns once all pending tasks have
  // completed.
  void Shutdown();

  // Posts |task| to worker pool. On completion, |reply|
  // is posted to the thread that called PostTaskAndReply().
  void PostTaskAndReply(const Callback& task, const base::Closure& reply);

  // Returns true when worker pool has reached its internal limit for number
  // of pending tasks.
  bool IsBusy();

  // Collect rendering stats all completed tasks.
  void GetRenderingStats(RenderingStats* stats);

 protected:
  class Worker : public base::Thread {
   public:
    Worker(WorkerPool* worker_pool, const std::string name);
    virtual ~Worker();

    // This must be called before the destructor.
    void StopAfterCompletingAllPendingTasks();

    // Posts a task to the worker thread.
    void PostTask(scoped_ptr<internal::WorkerPoolTask> task);

    int num_pending_tasks() const { return pending_tasks_.size(); }
    const RenderingStats& rendering_stats() const { return rendering_stats_; }

    // Overridden from base::Thread:
    virtual void Init() OVERRIDE;

   private:
    static void RunTask(internal::WorkerPoolTask* task);

    void OnTaskCompleted();

    WorkerPool* worker_pool_;
    base::WeakPtrFactory<Worker> weak_ptr_factory_;
    ScopedPtrDeque<internal::WorkerPoolTask> pending_tasks_;
    RenderingStats rendering_stats_;
  };

  explicit WorkerPool(size_t num_threads);

  WorkerPool::Worker* GetWorkerForNextTask();

 private:
  class NumPendingTasksComparator {
   public:
    bool operator() (const Worker* a, const Worker* b) const {
      return a->num_pending_tasks() < b->num_pending_tasks();
    }
  };

  void DidNumPendingTasksChange();
  void SortWorkersIfNeeded();

  typedef std::vector<Worker*> WorkerVector;
  WorkerVector workers_;
  bool workers_need_sorting_;
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(WorkerPool);
};

}  // namespace cc

#endif  // CC_WORKER_POOL_H_
