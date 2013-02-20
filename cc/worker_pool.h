// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_WORKER_POOL_H_
#define CC_WORKER_POOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "cc/rendering_stats.h"
#include "cc/scoped_ptr_deque.h"

namespace cc {
namespace internal {

class WorkerPoolTask {
 public:
  virtual ~WorkerPoolTask();

  // Called when the task is scheduled to run on a thread that isn't the
  // origin thread. Called on the origin thread.
  virtual void WillRunOnThread(base::Thread* thread) = 0;

  virtual void Run(RenderingStats* rendering_stats) = 0;

  bool HasCompleted();
  void DidComplete();

 protected:
  WorkerPoolTask(const base::Closure& reply);

  const base::Closure reply_;

  // Accessed from multiple threads. Set to 1 when task has completed.
  base::subtle::Atomic32 completed_;
};

}  // namespace internal

class CC_EXPORT WorkerPoolClient {
 public:
  virtual void DidFinishDispatchingWorkerPoolCompletionCallbacks() = 0;

 protected:
  virtual ~WorkerPoolClient() {}
};

// A worker thread pool that runs rendering tasks and guarantees completion
// of all pending tasks at shutdown.
class WorkerPool {
 public:
  typedef base::Callback<void(RenderingStats*)> Callback;

  virtual ~WorkerPool();

  static scoped_ptr<WorkerPool> Create(
      WorkerPoolClient* client, size_t num_threads) {
    return make_scoped_ptr(new WorkerPool(client, num_threads));
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

  // Toggle rendering stats collection.
  void SetRecordRenderingStats(bool record_rendering_stats);

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

    // Check for completed tasks and run reply callbacks.
    void CheckForCompletedTasks();

    int num_pending_tasks() const { return pending_tasks_.size(); }
    void set_record_rendering_stats(bool record_rendering_stats) {
      record_rendering_stats_ = record_rendering_stats;
    }
    const RenderingStats* rendering_stats() const {
      return rendering_stats_.get();
    }

    // Overridden from base::Thread:
    virtual void Init() OVERRIDE;

   private:
    static void RunTask(
        internal::WorkerPoolTask* task,
        WorkerPool* worker_pool,
        RenderingStats* rendering_stats);

    void OnTaskCompleted();

    WorkerPool* worker_pool_;
    ScopedPtrDeque<internal::WorkerPoolTask> pending_tasks_;
    scoped_ptr<RenderingStats> rendering_stats_;
    bool record_rendering_stats_;
  };

  WorkerPool(WorkerPoolClient* client, size_t num_threads);

  void PostTask(scoped_ptr<internal::WorkerPoolTask> task, bool is_cheap);

 private:
  class NumPendingTasksComparator {
   public:
    bool operator() (const Worker* a, const Worker* b) const {
      return a->num_pending_tasks() < b->num_pending_tasks();
    }
  };

  // Schedule a completed tasks check if not already pending.
  void ScheduleCheckForCompletedTasks();

  // Called on worker thread after completing work.
  void OnWorkCompletedOnWorkerThread();

  // Called on origin thread after becoming idle.
  void OnIdle();

  // Check for completed tasks and run reply callbacks.
  void CheckForCompletedTasks();

  // Called when processing task completion.
  void OnTaskCompleted();

  // Ensure workers are sorted by number of pending tasks.
  void SortWorkersIfNeeded();

  // Schedule running cheap tasks on the origin thread unless already pending.
  void ScheduleRunCheapTasks();

  // Run pending cheap tasks on the origin thread. If the allotted time slot
  // for cheap tasks runs out, the remaining tasks are deferred to the thread
  // pool.
  void RunCheapTasks();

  WorkerPool::Worker* GetWorkerForNextTask();
  bool CanPostCheapTask() const;

  typedef std::vector<Worker*> WorkerVector;
  WorkerVector workers_;
  WorkerPoolClient* client_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  base::WeakPtrFactory<WorkerPool> weak_ptr_factory_;
  bool workers_need_sorting_;
  bool shutdown_;
  base::CancelableClosure check_for_completed_tasks_callback_;
  base::TimeTicks check_for_completed_tasks_deadline_;
  base::Closure idle_callback_;
  base::Closure cheap_task_callback_;
  // Accessed from multiple threads. 0 when worker pool is idle.
  base::subtle::Atomic32 pending_task_count_;

  bool run_cheap_tasks_pending_;
  ScopedPtrDeque<internal::WorkerPoolTask> pending_cheap_tasks_;
  ScopedPtrDeque<internal::WorkerPoolTask> completed_cheap_tasks_;
  scoped_ptr<RenderingStats> cheap_rendering_stats_;

  DISALLOW_COPY_AND_ASSIGN(WorkerPool);
};

}  // namespace cc

#endif  // CC_WORKER_POOL_H_
