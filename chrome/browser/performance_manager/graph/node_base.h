// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/properties.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class Graph;

// NodeBase implements shared functionality among different types of
// coordination units. A specific type of coordination unit will derive from
// this class and can override shared funtionality when needed.
// All node classes allow construction on one sequence and subsequent use from
// another sequence.
// All methods not documented otherwise are single-threaded.
class NodeBase {
 public:
  NodeBase(resource_coordinator::CoordinationUnitType type, Graph* graph);
  virtual ~NodeBase();

  void AddObserver(GraphObserver* observer);
  void RemoveObserver(GraphObserver* observer);

  // May be called on any sequence.
  const resource_coordinator::CoordinationUnitID& id() const { return id_; }
  // May be called on any sequence.
  Graph* graph() const { return graph_; }

  const base::ObserverList<GraphObserver>::Unchecked& observers() const {
    return observers_;
  }

 protected:
  friend class Graph;

  // Called just before joining |graph_|, a good opportunity to initialize
  // node state.
  virtual void JoinGraph();
  // Called just before leaving |graph_|, a good opportunity to uninitialize
  // node state.
  virtual void LeaveGraph();

  // Returns true if |other_node| is in the same graph.
  bool NodeInGraph(const NodeBase* other_node) const;

  Graph* const graph_;
  const resource_coordinator::CoordinationUnitID id_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  base::ObserverList<GraphObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(NodeBase);
};

template <class NodeClass>
class TypedNodeBase : public NodeBase {
 public:
  using ObservedProperty = ObservedPropertyImpl<NodeClass, GraphObserver>;

  explicit TypedNodeBase(Graph* graph) : NodeBase(NodeClass::Type(), graph) {}

  static const NodeClass* FromNodeBase(const NodeBase* cu) {
    DCHECK(cu->id().type == NodeClass::Type());
    return static_cast<const NodeClass*>(cu);
  }

  static NodeClass* FromNodeBase(NodeBase* cu) {
    DCHECK(cu->id().type == NodeClass::Type());
    return static_cast<NodeClass*>(cu);
  }

  static NodeClass* GetNodeByID(
      Graph* graph,
      const resource_coordinator::CoordinationUnitID cu_id) {
    DCHECK(cu_id.type == NodeClass::Type());
    auto* cu = graph->GetNodeByID(cu_id);
    if (!cu)
      return nullptr;

    CHECK_EQ(cu->id().type, NodeClass::Type());
    return static_cast<NodeClass*>(cu);
  }
};

template <class NodeClass, class MojoInterfaceClass, class MojoRequestClass>
class CoordinationUnitInterface : public TypedNodeBase<NodeClass>,
                                  public MojoInterfaceClass {
 public:
  explicit CoordinationUnitInterface(Graph* graph)
      : TypedNodeBase<NodeClass>(graph) {}

  ~CoordinationUnitInterface() override = default;

  void AddBinding(MojoRequestClass request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  mojo::BindingSet<MojoInterfaceClass> bindings_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitInterface);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
