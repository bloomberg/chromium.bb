// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_GRAPH_RUNNER_H_
#define CC_RASTER_TASK_GRAPH_RUNNER_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"

namespace cc {

class TaskGraphRunner;

// A task which can be run by a TaskGraphRunner. To run a Task, it should be
// inserted into a TaskGraph, which can then be scheduled on the
// TaskGraphRunner.
class CC_EXPORT Task : public base::RefCountedThreadSafe<Task> {
 public:
  typedef std::vector<scoped_refptr<Task>> Vector;

  // Subclasses should implement this method. RunOnWorkerThread may be called
  // on any thread, and subclasses are responsible for locking and thread
  // safety.
  virtual void RunOnWorkerThread() = 0;

  void WillRun();
  void DidRun();
  bool HasFinishedRunning() const;

 protected:
  friend class base::RefCountedThreadSafe<Task>;

  Task();
  virtual ~Task();

  bool will_run_;
  bool did_run_;
};

// A task dependency graph describes the order in which to execute a set
// of tasks. Dependencies are represented as edges. Each node is assigned
// a category, a priority and a run count that matches the number of
// dependencies. Priority range from 0 (most favorable scheduling) to UINT16_MAX
// (least favorable). Categories range from 0 to UINT16_MAX. It is up to the
// implementation and its consumer to determine the meaning (if any) of a
// category. A TaskGraphRunner implementation may chose to prioritize certain
// categories over others, regardless of the individual priorities of tasks.
struct CC_EXPORT TaskGraph {
  struct Node {
    typedef std::vector<Node> Vector;

    Node(Task* task,
         uint16_t category,
         uint16_t priority,
         uint32_t dependencies)
        : task(task),
          category(category),
          priority(priority),
          dependencies(dependencies) {}

    Task* task;
    uint16_t category;
    uint16_t priority;
    uint32_t dependencies;
  };

  struct Edge {
    typedef std::vector<Edge> Vector;

    Edge(const Task* task, Task* dependent)
        : task(task), dependent(dependent) {}

    const Task* task;
    Task* dependent;
  };

  TaskGraph();
  ~TaskGraph();

  void Swap(TaskGraph* other);
  void Reset();

  Node::Vector nodes;
  Edge::Vector edges;
};

// Opaque identifier that defines a namespace of tasks.
class CC_EXPORT NamespaceToken {
 public:
  NamespaceToken() : id_(0) {}
  ~NamespaceToken() {}

  bool IsValid() const { return id_ != 0; }

 private:
  friend class TaskGraphWorkQueue;

  explicit NamespaceToken(int id) : id_(id) {}

  int id_;
};

// A TaskGraphRunner is an object that runs a set of tasks in the
// order defined by a dependency graph. The TaskGraphRunner interface
// provides a way of decoupling task scheduling from the mechanics of
// how each task will be run. TaskGraphRunner provides the following
// guarantees:
//
//   - Scheduled tasks will not run synchronously. That is, the
//     ScheduleTasks() method will not call Task::Run() directly.
//
//   - Scheduled tasks are guaranteed to run in the order defined by
//     dependency graph.
//
//   - Once scheduled, a task is guaranteed to end up in the
//     |completed_tasks| vector populated by CollectCompletedTasks(),
//     even if it later gets canceled by another call to ScheduleTasks().
//
// TaskGraphRunner does not guarantee that a task with high priority
// runs after a task with low priority. The priority is just a hint
// and a valid TaskGraphRunner might ignore it. Also it does not
// guarantee a memory model for shared data between tasks. (In other
// words, you should use your own synchronization/locking primitives if
// you need to share data between tasks.)
//
// Implementations of TaskGraphRunner should be thread-safe in that all
// methods must be safe to call on any thread.
//
// Some theoretical implementations of TaskGraphRunner:
//
//   - A TaskGraphRunner that uses a thread pool to run scheduled tasks.
//
//   - A TaskGraphRunner that has a method Run() that runs each task.
class CC_EXPORT TaskGraphRunner {
 public:
  // Returns a unique token that can be used to pass a task graph to
  // ScheduleTasks(). Valid tokens are always nonzero.
  virtual NamespaceToken GetNamespaceToken() = 0;

  // Schedule running of tasks in |graph|. Tasks previously scheduled but no
  // longer needed will be canceled unless already running. Canceled tasks are
  // moved to |completed_tasks| without being run. The result is that once
  // scheduled, a task is guaranteed to end up in the |completed_tasks| queue
  // even if it later gets canceled by another call to ScheduleTasks().
  virtual void ScheduleTasks(NamespaceToken token, TaskGraph* graph) = 0;

  // Wait for all scheduled tasks to finish running.
  virtual void WaitForTasksToFinishRunning(NamespaceToken token) = 0;

  // Collect all completed tasks in |completed_tasks|.
  virtual void CollectCompletedTasks(NamespaceToken token,
                                     Task::Vector* completed_tasks) = 0;

 protected:
  virtual ~TaskGraphRunner() {}
};

// Helper class for iterating over all dependents of a task.
class DependentIterator {
 public:
  DependentIterator(TaskGraph* graph, const Task* task)
      : graph_(graph),
        task_(task),
        current_index_(static_cast<size_t>(-1)),
        current_node_(NULL) {
    ++(*this);
  }

  TaskGraph::Node& operator->() const {
    DCHECK_LT(current_index_, graph_->edges.size());
    DCHECK_EQ(graph_->edges[current_index_].task, task_);
    DCHECK(current_node_);
    return *current_node_;
  }

  TaskGraph::Node& operator*() const {
    DCHECK_LT(current_index_, graph_->edges.size());
    DCHECK_EQ(graph_->edges[current_index_].task, task_);
    DCHECK(current_node_);
    return *current_node_;
  }

  // Note: Performance can be improved by keeping edges sorted.
  DependentIterator& operator++() {
    // Find next dependency edge for |task_|.
    do {
      ++current_index_;
      if (current_index_ == graph_->edges.size())
        return *this;
    } while (graph_->edges[current_index_].task != task_);

    // Now find the node for the dependent of this edge.
    TaskGraph::Node::Vector::iterator it = std::find_if(
        graph_->nodes.begin(), graph_->nodes.end(),
        [this](const TaskGraph::Node& node) {
          return node.task == graph_->edges[current_index_].dependent;
        });
    DCHECK(it != graph_->nodes.end());
    current_node_ = &(*it);

    return *this;
  }

  operator bool() const { return current_index_ < graph_->edges.size(); }

 private:
  TaskGraph* graph_;
  const Task* task_;
  size_t current_index_;
  TaskGraph::Node* current_node_;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_GRAPH_RUNNER_H_
