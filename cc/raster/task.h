// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_H_
#define CC_RASTER_TASK_H_

#include <stdint.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"

namespace cc {

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
  TaskGraph(const TaskGraph& other);
  ~TaskGraph();

  void Swap(TaskGraph* other);
  void Reset();

  Node::Vector nodes;
  Edge::Vector edges;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_H_
