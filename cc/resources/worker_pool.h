// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_WORKER_POOL_H_
#define CC_RESOURCES_WORKER_POOL_H_

#include <deque>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "cc/base/cc_export.h"

namespace cc {
namespace internal {

class CC_EXPORT WorkerPoolTask
    : public base::RefCountedThreadSafe<WorkerPoolTask> {
 public:
  virtual void RunOnWorkerThread(unsigned thread_index) = 0;
  virtual void CompleteOnOriginThread() = 0;

  void DidSchedule();
  void WillRun();
  void DidRun();
  void WillComplete();
  void DidComplete();

  bool HasFinishedRunning() const;
  bool HasCompleted() const;

 protected:
  friend class base::RefCountedThreadSafe<WorkerPoolTask>;

  WorkerPoolTask();
  virtual ~WorkerPoolTask();

 private:
  bool did_schedule_;
  bool did_run_;
  bool did_complete_;
};

class CC_EXPORT GraphNode {
 public:
  typedef std::vector<GraphNode*> Vector;

  GraphNode(internal::WorkerPoolTask* task, unsigned priority);
  ~GraphNode();

  WorkerPoolTask* task() { return task_; }

  void add_dependent(GraphNode* dependent) {
    DCHECK(dependent);
    dependents_.push_back(dependent);
  }
  const Vector& dependents() const { return dependents_; }

  unsigned priority() const { return priority_; }

  unsigned num_dependencies() const { return num_dependencies_; }
  void add_dependency() { ++num_dependencies_; }
  void remove_dependency() {
    DCHECK(num_dependencies_);
    --num_dependencies_;
  }

 private:
  WorkerPoolTask* task_;
  Vector dependents_;
  unsigned priority_;
  unsigned num_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(GraphNode);
};

}  // namespace internal
}  // namespace cc

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <> struct hash<cc::internal::WorkerPoolTask*> {
  size_t operator()(cc::internal::WorkerPoolTask* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace cc {

// A worker thread pool that runs tasks provided by task graph and
// guarantees completion of all pending tasks at shutdown.
class CC_EXPORT WorkerPool {
 public:
  virtual ~WorkerPool();

  // Tells the worker pool to shutdown and returns once all pending tasks have
  // completed.
  virtual void Shutdown();

  // Force a check for completed tasks.
  virtual void CheckForCompletedTasks();

 protected:
  // A task graph contains a unique set of tasks with edges between
  // dependencies pointing in the direction of the dependents. Each task
  // need to be assigned a unique priority and a run count that matches
  // the number of dependencies.
  typedef base::ScopedPtrHashMap<internal::WorkerPoolTask*, internal::GraphNode>
      GraphNodeMap;
  typedef GraphNodeMap TaskGraph;

  WorkerPool(size_t num_threads, const std::string& thread_name_prefix);

  // Schedule running of tasks in |graph|. Any previously scheduled tasks
  // that are not already running will be canceled. Canceled tasks don't run
  // but completion of them is still processed.
  void SetTaskGraph(TaskGraph* graph);

 private:
  class Inner;
  friend class Inner;

  typedef std::vector<scoped_refptr<internal::WorkerPoolTask> > TaskVector;

  void ProcessCompletedTasks(const TaskVector& completed_tasks);

  bool in_dispatch_completion_callbacks_;

  // Hide the gory details of the worker pool in |inner_|.
  const scoped_ptr<Inner> inner_;
};

}  // namespace cc

#endif  // CC_RESOURCES_WORKER_POOL_H_
