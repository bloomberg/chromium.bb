// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_

#include <cstdint>

#include "base/macros.h"

namespace performance_manager {

class GraphObserver;
class FrameNodeObserver;
class PageNodeObserver;
class ProcessNodeObserver;
class SystemNodeObserver;

// Represents a graph of the nodes representing a single browser. Maintains a
// set of nodes that can be retrieved in different ways, some indexed. Keeps
// a list of observers that are notified of node addition and removal.
class Graph {
 public:
  using Observer = GraphObserver;

  Graph();
  virtual ~Graph();

  // Adds an |observer| on the graph. It is safe for observers to stay
  // registered on the graph at the time of its death.
  virtual void AddGraphObserver(GraphObserver* observer) = 0;
  virtual void AddFrameNodeObserver(FrameNodeObserver* observer) = 0;
  virtual void AddPageNodeObserver(PageNodeObserver* observer) = 0;
  virtual void AddProcessNodeObserver(ProcessNodeObserver* observer) = 0;
  virtual void AddSystemNodeObserver(SystemNodeObserver* observer) = 0;

  // Removes an |observer| from the graph.
  virtual void RemoveGraphObserver(GraphObserver* observer) = 0;
  virtual void RemoveFrameNodeObserver(FrameNodeObserver* observer) = 0;
  virtual void RemovePageNodeObserver(PageNodeObserver* observer) = 0;
  virtual void RemoveProcessNodeObserver(ProcessNodeObserver* observer) = 0;
  virtual void RemoveSystemNodeObserver(SystemNodeObserver* observer) = 0;

  // The following functions are implementation detail and should not need to be
  // used by external clients. They provide the ability to safely downcast to
  // the underlying implementation.
  virtual uintptr_t GetImplType() const = 0;
  virtual const void* GetImpl() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Graph);
};

// Observer interface for the graph.
class GraphObserver {
 public:
  GraphObserver();
  virtual ~GraphObserver();

  // Called before the |graph| associated with this observer disappears. This
  // allows the observer to do any necessary cleanup work. Note that the graph
  // is in its destructor while this is being called, so the observer should
  // refrain from uselessly modifying the graph. This is intended to be used to
  // facilitate lifetime management of observers.
  // TODO(chrisha): Make this run before the constructor!
  // crbug.com/966840
  virtual void OnBeforeGraphDestroyed(const Graph* graph) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_GRAPH_H_
