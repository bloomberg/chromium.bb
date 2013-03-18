// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_WORKER_POOL_H_
#define CC_BASE_WORKER_POOL_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {
struct RenderingStats;

namespace internal {

class WorkerPoolTask {
 public:
  virtual ~WorkerPoolTask();

  virtual bool IsCheap() = 0;

  virtual void Run(RenderingStats* rendering_stats) = 0;

  virtual void RunOnThread(
      RenderingStats* rendering_stats, unsigned thread_index) = 0;

  void DidComplete();

 protected:
  WorkerPoolTask(const base::Closure& reply);

  const base::Closure reply_;
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
      WorkerPoolClient* client,
      size_t num_threads,
      base::TimeDelta check_for_completed_tasks_delay,
      const std::string& thread_name_prefix) {
    return make_scoped_ptr(new WorkerPool(client,
                                          num_threads,
                                          check_for_completed_tasks_delay,
                                          thread_name_prefix));
  }

  // Tells the worker pool to shutdown and returns once all pending tasks have
  // completed.
  void Shutdown();

  // Posts |task| to worker pool. On completion, |reply|
  // is posted to the thread that called PostTaskAndReply().
  void PostTaskAndReply(const Callback& task, const base::Closure& reply);

  // Set time limit for running cheap tasks.
  void SetRunCheapTasksTimeLimit(base::TimeTicks run_cheap_tasks_time_limit);

  // Toggle rendering stats collection.
  void SetRecordRenderingStats(bool record_rendering_stats);

  // Collect rendering stats of all completed tasks.
  void GetRenderingStats(RenderingStats* stats);

 protected:
  WorkerPool(WorkerPoolClient* client,
             size_t num_threads,
             base::TimeDelta check_for_completed_tasks_delay,
             const std::string& thread_name_prefix);

  void PostTask(scoped_ptr<internal::WorkerPoolTask> task);

 private:
  class Inner;
  friend class Inner;

  void OnTaskCompleted();
  void OnIdle();
  void ScheduleCheckForCompletedTasks();
  void CheckForCompletedTasks();
  void CancelCheckForCompletedTasks();
  void DispatchCompletionCallbacks();
  void ScheduleRunCheapTasks();
  void RunCheapTasks();

  WorkerPoolClient* client_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  base::WeakPtrFactory<WorkerPool> weak_ptr_factory_;
  base::TimeTicks check_for_completed_tasks_time_;
  base::TimeDelta check_for_completed_tasks_delay_;
  base::CancelableClosure check_for_completed_tasks_callback_;
  bool check_for_completed_tasks_pending_;
  const base::Closure run_cheap_tasks_callback_;
  base::TimeTicks run_cheap_tasks_time_limit_;
  bool run_cheap_tasks_pending_;

  // Holds all completed tasks for which we have not yet dispatched
  // reply callbacks.
  ScopedPtrDeque<internal::WorkerPoolTask> completed_tasks_;

  // Hide the gory details of the worker pool in |inner_|.
  const scoped_ptr<Inner> inner_;

  DISALLOW_COPY_AND_ASSIGN(WorkerPool);
};

}  // namespace cc

#endif  // CC_BASE_WORKER_POOL_H_
