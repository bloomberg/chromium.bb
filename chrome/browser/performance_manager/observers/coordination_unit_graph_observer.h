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
//   (2) Register by calling on |coordination_unit_graph().RegisterObserver|
//       inside of the ResourceCoordinatorService::Create.
class GraphObserver {
 public:
  GraphObserver();
  virtual ~GraphObserver();

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |coordination_unit|.
  virtual bool ShouldObserve(const NodeBase* coordination_unit) = 0;

  // Called whenever a CoordinationUnit is created.
  virtual void OnNodeCreated(const NodeBase* coordination_unit) {}

  // Called when the |coordination_unit| is about to be destroyed.
  virtual void OnBeforeNodeDestroyed(const NodeBase* coordination_unit) {}

  // Called whenever a property of the |coordination_unit| is changed if the
  // |coordination_unit| doesn't implement its own PropertyChanged handler.
  virtual void OnPropertyChanged(
      const NodeBase* coordination_unit,
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the FrameNode is changed.
  virtual void OnFramePropertyChanged(
      const FrameNodeImpl* frame_cu,
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the PageCoordinationUnit is changed.
  virtual void OnPagePropertyChanged(
      const PageNodeImpl* page_cu,
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the ProcessCoordinationUnit is changed.
  virtual void OnProcessPropertyChanged(
      const ProcessNodeImpl* process_cu,
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the SystemCoordinationUnit is changed.
  virtual void OnSystemPropertyChanged(
      const SystemNodeImpl* system_cu,
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever an event is received in |coordination_unit| if the
  // |coordination_unit| doesn't implement its own EventReceived handler.
  virtual void OnEventReceived(const NodeBase* coordination_unit,
                               const resource_coordinator::mojom::Event event) {
  }
  virtual void OnFrameEventReceived(
      const FrameNodeImpl* frame_cu,
      const resource_coordinator::mojom::Event event) {}
  virtual void OnPageEventReceived(
      const PageNodeImpl* page_cu,
      const resource_coordinator::mojom::Event event) {}
  virtual void OnProcessEventReceived(
      const ProcessNodeImpl* process_cu,
      const resource_coordinator::mojom::Event event) {}
  virtual void OnSystemEventReceived(
      const SystemNodeImpl* system_cu,
      const resource_coordinator::mojom::Event event) {}

  // Called when all the frames in a process become frozen.
  virtual void OnAllFramesInProcessFrozen(const ProcessNodeImpl* process_cu) {}

  void set_coordination_unit_graph(Graph* coordination_unit_graph) {
    coordination_unit_graph_ = coordination_unit_graph;
  }

  const Graph& coordination_unit_graph() const {
    return *coordination_unit_graph_;
  }

 private:
  Graph* coordination_unit_graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GraphObserver);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_OBSERVERS_COORDINATION_UNIT_GRAPH_OBSERVER_H_
