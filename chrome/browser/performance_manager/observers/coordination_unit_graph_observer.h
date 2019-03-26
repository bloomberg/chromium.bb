// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_COORDINATION_UNIT_GRAPH_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_COORDINATION_UNIT_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class FrameNodeImpl;
class Graph;
class NodeBase;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// An observer API for the coordination unit graph maintained by GRC.
//
// Observers are instantiated when the resource_coordinator service
// is created and are destroyed when the resource_coordinator service
// is destroyed. Therefore observers are guaranteed to be alive before
// any coordination unit is created and will be alive after any
// coordination unit is destroyed. Additionally, any
// Coordination Unit reachable within a callback will always be
// initialized and valid.
//
// To create and install a new observer:
//   (1) Derive from this class.
//   (2) Register by calling on |graph().RegisterObserver|
//       inside of the ResourceCoordinatorService::Create.
//
// TODO: Clean up the observer API, and create a wrapper version that sees
// const Node* rather then mutable NodeImpl* types for external consumers.
class GraphObserver {
 public:
  GraphObserver();
  virtual ~GraphObserver();

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |node|.
  virtual bool ShouldObserve(const NodeBase* node) = 0;

  // Called whenever a CoordinationUnit is created.
  virtual void OnNodeAdded(NodeBase* node) {}

  // Called when the |node| is about to be destroyed.
  virtual void OnBeforeNodeRemoved(NodeBase* node) {}

  // Called whenever a property of the |node| is changed if the
  // |node| doesn't implement its own PropertyChanged handler.
  virtual void OnPropertyChanged(
      NodeBase* node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the FrameNode is changed.
  virtual void OnFramePropertyChanged(
      FrameNodeImpl* frame_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the PageCoordinationUnit is changed.
  virtual void OnPagePropertyChanged(
      PageNodeImpl* page_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the ProcessCoordinationUnit is changed.
  virtual void OnProcessPropertyChanged(
      ProcessNodeImpl* process_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the SystemCoordinationUnit is changed.
  virtual void OnSystemPropertyChanged(
      SystemNodeImpl* system_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever an event is received in |node| if the
  // |node| doesn't implement its own EventReceived handler.
  virtual void OnEventReceived(NodeBase* node,
                               resource_coordinator::mojom::Event event) {}
  virtual void OnFrameEventReceived(FrameNodeImpl* frame_node,
                                    resource_coordinator::mojom::Event event) {}
  virtual void OnPageEventReceived(PageNodeImpl* page_node,
                                   resource_coordinator::mojom::Event event) {}
  virtual void OnProcessEventReceived(
      ProcessNodeImpl* process_node,
      resource_coordinator::mojom::Event event) {}
  virtual void OnSystemEventReceived(SystemNodeImpl* system_node,
                                     resource_coordinator::mojom::Event event) {
  }

  // FrameNodeImpl notifications.
  virtual void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) {}

  // PageNodeImpl notifications.
  virtual void OnIsVisibleChanged(PageNodeImpl* page_node) {}
  virtual void OnIsLoadingChanged(PageNodeImpl* page_node) {}

  // ProcessNodeImpl notifications.
  virtual void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) {}

  // Called when page almost idle state changes. This is a computed property and
  // will only be maintained if a PageAlmostIdleDecorator exists on the graph.
  virtual void OnPageAlmostIdleChanged(PageNodeImpl* page_node) {}

  // Called when all the frames in a process become frozen.
  virtual void OnAllFramesInProcessFrozen(ProcessNodeImpl* process_node) {}

  void set_node_graph(Graph* graph) { node_graph_ = graph; }

  const Graph& graph() const { return *node_graph_; }

 private:
  Graph* node_graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_COORDINATION_UNIT_GRAPH_OBSERVER_H_
