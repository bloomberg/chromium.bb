// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_WORKER_POOL_H_
#define CC_RESOURCES_WORKER_POOL_H_

#include <deque>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_hash_map.h"

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
  class CC_EXPORT GraphNode {
   public:
    GraphNode(internal::WorkerPoolTask* dependent, unsigned priority);
    ~GraphNode();

    void AddDependent(internal::WorkerPoolTask* dependent);

    const internal::WorkerPoolTask::TaskVector& dependents() const {
      return dependents_;
    }
    unsigned priority() const { return priority_; }

   private:
    internal::WorkerPoolTask::TaskVector dependents_;
    unsigned priority_;

    DISALLOW_COPY_AND_ASSIGN(GraphNode);
  };
  typedef ScopedPtrHashMap<internal::WorkerPoolTask*, GraphNode> GraphNodeMap;
  typedef GraphNodeMap TaskGraph;

  WorkerPool(size_t num_threads, const std::string& thread_name_prefix);

  // Schedule running of tasks in |graph|. Any previously scheduled tasks
  // that are not already running will be canceled. Canceled tasks don't run
  // but completion of them is still processed.
  void SetTaskGraph(TaskGraph* graph);

  // BuildTaskGraph() takes a task tree as input and constructs a
  // unique set of tasks with edges between dependencies pointing in
  // the direction of the dependents. Each task is given a unique priority
  // which is currently the same as the DFS traversal order.
  //
  // Input:             Output:
  //
  //       root               task4          Task | Priority (lower is better)
  //     /      \           /       \      -------+---------------------------
  //  task1    task2      task3    task2     root | 4
  //    |        |          |        |      task1 | 2
  //  task3      |        task1      |      task2 | 3
  //    |        |           \      /       task3 | 1
  //  task4    task4           root         task4 | 0
  //
  // The output can be used to efficiently maintain a queue of
  // "ready to run" tasks.
  static unsigned BuildTaskGraphRecursive(
      internal::WorkerPoolTask* task,
      internal::WorkerPoolTask* dependent,
      unsigned priority,
      TaskGraph* tasks);
  static void BuildTaskGraph(
      internal::WorkerPoolTask* root, TaskGraph* tasks);

 private:
  class Inner;
  friend class Inner;

  typedef std::deque<scoped_refptr<internal::WorkerPoolTask> > TaskDeque;

  void DispatchCompletionCallbacks(TaskDeque* completed_tasks);

  bool in_dispatch_completion_callbacks_;

  // Hide the gory details of the worker pool in |inner_|.
  const scoped_ptr<Inner> inner_;
};

}  // namespace cc

#endif  // CC_RESOURCES_WORKER_POOL_H_
