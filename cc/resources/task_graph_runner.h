// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TASK_GRAPH_RUNNER_H_
#define CC_RESOURCES_TASK_GRAPH_RUNNER_H_

#include <queue>
#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/simple_thread.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_deque.h"

namespace cc {
namespace internal {

class CC_EXPORT Task : public base::RefCountedThreadSafe<Task> {
 public:
  typedef std::vector<scoped_refptr<Task> > Vector;

  virtual void RunOnWorkerThread(unsigned thread_index) = 0;

  void DidSchedule();
  void WillRun();
  void DidRun();

  bool HasFinishedRunning() const;

 protected:
  friend class base::RefCountedThreadSafe<Task>;

  Task();
  virtual ~Task();

  bool did_schedule_;
  bool did_run_;
};

}  // namespace internal
}  // namespace cc

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
struct hash<cc::internal::Task*> {
  size_t operator()(cc::internal::Task* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace cc {
namespace internal {

// Dependencies are represented by edges in a task graph. A graph node
// store edges as a vector of dependents. Each graph node is assigned
// a priority and a run count that matches the number of dependencies.
class CC_EXPORT GraphNode {
 public:
  typedef std::vector<GraphNode*> Vector;
  typedef base::ScopedPtrHashMap<Task*, GraphNode> Map;

  GraphNode(Task* task, unsigned priority);
  ~GraphNode();

  Task* task() { return task_; }

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
  Task* task_;
  Vector dependents_;
  unsigned priority_;
  unsigned num_dependencies_;

  DISALLOW_COPY_AND_ASSIGN(GraphNode);
};

class TaskGraphRunner;

// Opaque identifier that defines a namespace of tasks.
class CC_EXPORT NamespaceToken {
 public:
  NamespaceToken() : id_(0) {}
  ~NamespaceToken() {}

  bool IsValid() const { return id_ != 0; }

 private:
  friend class TaskGraphRunner;

  explicit NamespaceToken(int id) : id_(id) {}

  int id_;
};

// A worker thread pool that runs tasks provided by task graph. Destructor
// might block and should not be used on a thread that needs to be responsive.
class CC_EXPORT TaskGraphRunner : public base::DelegateSimpleThread::Delegate {
 public:
  typedef GraphNode::Map TaskGraph;

  TaskGraphRunner(size_t num_threads, const std::string& thread_name_prefix);
  virtual ~TaskGraphRunner();

  // Returns a unique token that can be used to pass a task graph to
  // SetTaskGraph(). Valid tokens are always nonzero.
  NamespaceToken GetNamespaceToken();

  // Schedule running of tasks in |graph|. Tasks previously scheduled but
  // no longer needed will be canceled unless already running. Canceled
  // tasks are moved to |completed_tasks| without being run. The result
  // is that once scheduled, a task is guaranteed to end up in the
  // |completed_tasks| queue even if it later get canceled by another
  // call to SetTaskGraph().
  void SetTaskGraph(NamespaceToken token, TaskGraph* graph);

  // Wait for all scheduled tasks to finish running.
  void WaitForTasksToFinishRunning(NamespaceToken token);

  // Collect all completed tasks in |completed_tasks|.
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks);

 private:
  struct TaskNamespace {
    typedef std::vector<TaskNamespace*> Vector;

    TaskNamespace();
    ~TaskNamespace();

    // This set contains all pending tasks.
    TaskGraph pending_tasks;
    // This set contains all currently running tasks.
    TaskGraph running_tasks;
    // Completed tasks not yet collected by origin thread.
    Task::Vector completed_tasks;
    // Ordered set of tasks that are ready to run.
    internal::GraphNode::Vector ready_to_run_tasks;
  };

  typedef base::ScopedPtrHashMap<int, TaskNamespace> TaskNamespaceMap;

  static bool CompareTaskPriority(const internal::GraphNode* a,
                                  const internal::GraphNode* b) {
    // In this system, numerically lower priority is run first.
    if (a->priority() != b->priority())
      return a->priority() > b->priority();

    // Run task with most dependents first when priority is the same.
    return a->dependents().size() < b->dependents().size();
  }

  static bool CompareTaskNamespacePriority(const TaskNamespace* a,
                                           const TaskNamespace* b) {
    DCHECK(!a->ready_to_run_tasks.empty());
    DCHECK(!b->ready_to_run_tasks.empty());

    // Compare based on task priority of the ready_to_run_tasks heap
    // .front() will hold the max element of the heap,
    // except after pop_heap, when max element is moved to .back().
    return CompareTaskPriority(a->ready_to_run_tasks.front(),
                               b->ready_to_run_tasks.front());
  }

  static bool HasFinishedRunningTasksInNamespace(
      TaskNamespace* task_namespace) {
    return task_namespace->pending_tasks.empty() &&
           task_namespace->running_tasks.empty();
  }

  // Overridden from base::DelegateSimpleThread:
  virtual void Run() OVERRIDE;

  // This lock protects all members of this class. Do not read or modify
  // anything without holding this lock. Do not block while holding this
  // lock.
  mutable base::Lock lock_;

  // Condition variable that is waited on by worker threads until new
  // tasks are ready to run or shutdown starts.
  base::ConditionVariable has_ready_to_run_tasks_cv_;

  // Condition variable that is waited on by origin threads until a
  // namespace has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;

  // Provides a unique id to each NamespaceToken.
  int next_namespace_id_;

  // This set contains all namespaces with pending, running or completed
  // tasks not yet collected.
  TaskNamespaceMap namespaces_;

  // Ordered set of task namespaces that have ready to run tasks.
  TaskNamespace::Vector ready_to_run_namespaces_;

  // Provides each running thread loop with a unique index. First thread
  // loop index is 0.
  unsigned next_thread_index_;

  // Set during shutdown. Tells workers to exit when no more tasks
  // are pending.
  bool shutdown_;

  ScopedPtrDeque<base::DelegateSimpleThread> workers_;

  DISALLOW_COPY_AND_ASSIGN(TaskGraphRunner);
};

}  // namespace internal
}  // namespace cc

#endif  // CC_RESOURCES_TASK_GRAPH_RUNNER_H_
