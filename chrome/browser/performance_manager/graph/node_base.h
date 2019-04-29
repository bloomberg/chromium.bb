// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/node_type.h"
#include "chrome/browser/performance_manager/graph/properties.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class Graph;

// NodeBase implements shared functionality among different types of graph
// nodes. A specific type of graph node will derive from this class and can
// override shared functionality when needed.
// All node classes allow construction on one sequence and subsequent use from
// another sequence.
// All methods not documented otherwise are single-threaded.
class NodeBase {
 public:
  // TODO(siggi): Don't store the node type, expose it on a virtual function
  //    instead.
  NodeBase(NodeTypeEnum type, Graph* graph);
  virtual ~NodeBase();

  void AddObserver(GraphObserver* observer);
  void RemoveObserver(GraphObserver* observer);

  // May be called on any sequence.
  NodeTypeEnum type() const { return type_; }

  // May be called on any sequence.
  Graph* graph() const { return graph_; }

  const base::ObserverList<GraphObserver>::Unchecked& observers() const {
    return observers_;
  }

  // Returns an opaque ID for |node|, unique across all nodes in the same graph,
  // zero for nullptr. This should never be used to look up nodes, only to
  // provide a stable ID for serialization.
  static int64_t GetSerializationId(NodeBase* node);

 protected:
  friend class Graph;

  // Called just before joining |graph_|, a good opportunity to initialize
  // node state.
  virtual void JoinGraph();
  // Called just before leaving |graph_|, a good opportunity to uninitialize
  // node state.
  virtual void LeaveGraph();

  Graph* const graph_;
  const NodeTypeEnum type_;

  // Assigned on first use, immutable from that point forward.
  int64_t serialization_id_ = 0u;

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

  static const NodeClass* FromNodeBase(const NodeBase* node) {
    DCHECK_EQ(node->type(), NodeClass::Type());
    return static_cast<const NodeClass*>(node);
  }

  static NodeClass* FromNodeBase(NodeBase* node) {
    DCHECK(node->type() == NodeClass::Type());
    return static_cast<NodeClass*>(node);
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
