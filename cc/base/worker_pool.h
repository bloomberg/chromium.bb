// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_WORKER_POOL_H_
#define CC_BASE_WORKER_POOL_H_

#include <deque>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "cc/base/cc_export.h"

namespace cc {

namespace internal {

class CC_EXPORT WorkerPoolTask
    : public base::RefCountedThreadSafe<WorkerPoolTask> {
 public:
  typedef std::vector<scoped_refptr<WorkerPoolTask> > TaskVector;

  virtual void RunOnThread(unsigned thread_index) = 0;
  virtual void DispatchCompletionCallback() = 0;

  void DidSchedule();
  void WillRun();
  void DidRun();
  void DidComplete();

  bool IsReadyToRun() const;
  bool HasFinishedRunning() const;
  bool HasCompleted() const;

  TaskVector& dependencies() { return dependencies_; }

 protected:
  friend class base::RefCountedThreadSafe<WorkerPoolTask>;

  WorkerPoolTask();
  explicit WorkerPoolTask(TaskVector* dependencies);
  virtual ~WorkerPoolTask();

 private:
  bool did_schedule_;
  bool did_run_;
  bool did_complete_;
  TaskVector dependencies_;
};

}  // namespace internal

class CC_EXPORT WorkerPoolClient {
 public:
  virtual void DidFinishDispatchingWorkerPoolCompletionCallbacks() = 0;

 protected:
  virtual ~WorkerPoolClient() {}
};

// A worker thread pool that runs tasks provided by task graph and
// guarantees completion of all pending tasks at shutdown.
class CC_EXPORT WorkerPool {
 public:
  virtual ~WorkerPool();

  // Tells the worker pool to shutdown and returns once all pending tasks have
  // completed.
  virtual void Shutdown();

  // Set a new client.
  void SetClient(WorkerPoolClient* client) {
    client_ = client;
  }

  // Force a check for completed tasks.
  virtual void CheckForCompletedTasks();

 protected:
  WorkerPool(size_t num_threads,
             base::TimeDelta check_for_completed_tasks_delay,
             const std::string& thread_name_prefix);

  void ScheduleTasks(internal::WorkerPoolTask* root);

 private:
  class Inner;
  friend class Inner;

  typedef std::deque<scoped_refptr<internal::WorkerPoolTask> > TaskDeque;

  void ScheduleCheckForCompletedTasks();
  void DispatchCompletionCallbacks(TaskDeque* completed_tasks);

  WorkerPoolClient* client_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  base::CancelableClosure check_for_completed_tasks_callback_;
  base::TimeDelta check_for_completed_tasks_delay_;
  bool check_for_completed_tasks_pending_;
  bool in_dispatch_completion_callbacks_;

  // Hide the gory details of the worker pool in |inner_|.
  const scoped_ptr<Inner> inner_;
};

}  // namespace cc

#endif  // CC_BASE_WORKER_POOL_H_
