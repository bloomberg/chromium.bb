// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_provider.mojom.h"

namespace performance_manager {

class Graph;

// NodeBase implements shared functionality among different types of
// coordination units. A specific type of coordination unit will derive from
// this class and can override shared funtionality when needed.
class NodeBase {
 public:
  NodeBase(const resource_coordinator::CoordinationUnitID& id, Graph* graph);
  virtual ~NodeBase();

  void Destruct();
  void BeforeDestroyed();
  void AddObserver(GraphObserver* observer);
  void RemoveObserver(GraphObserver* observer);
  bool GetProperty(
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t* result) const;
  int64_t GetPropertyOrDefault(
      const resource_coordinator::mojom::PropertyType property_type,
      int64_t default_value) const;

  const resource_coordinator::CoordinationUnitID& id() const { return id_; }
  Graph* graph() const { return graph_; }

  const base::ObserverList<GraphObserver>::Unchecked& observers() const {
    return observers_;
  }

  void SetPropertyForTesting(int64_t value) {
    SetProperty(resource_coordinator::mojom::PropertyType::kTest, value);
  }

  const std::map<resource_coordinator::mojom::PropertyType, int64_t>&
  properties_for_testing() const {
    return properties_;
  }

 protected:
  // Helper function for setting a property, and notifying observers if the
  // value has changed.
  template <typename NodeType,
            typename PropertyType,
            typename NotifyFunctionPtr>
  void SetPropertyAndNotifyObservers(NotifyFunctionPtr notify_function_ptr,
                                     const PropertyType& value,
                                     NodeType* node,
                                     PropertyType* property) {
    if (*property == value)
      return;
    *property = value;
    for (auto& observer : observers_)
      ((observer).*(notify_function_ptr))(node);
  }

  virtual void OnEventReceived(resource_coordinator::mojom::Event event);
  virtual void OnPropertyChanged(
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value);

  void SendEvent(resource_coordinator::mojom::Event event);
  void SetProperty(resource_coordinator::mojom::PropertyType property_type,
                   int64_t value);

  // Passes the ownership of the newly created |new_cu| to its graph.
  static NodeBase* PassOwnershipToGraph(std::unique_ptr<NodeBase> new_cu);

  Graph* const graph_;
  const resource_coordinator::CoordinationUnitID id_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  base::ObserverList<GraphObserver>::Unchecked observers_;
  std::map<resource_coordinator::mojom::PropertyType, int64_t> properties_;

  DISALLOW_COPY_AND_ASSIGN(NodeBase);
};

template <class CoordinationUnitClass,
          class MojoInterfaceClass,
          class MojoRequestClass>
class CoordinationUnitInterface : public NodeBase, public MojoInterfaceClass {
 public:
  static CoordinationUnitClass* Create(
      const resource_coordinator::CoordinationUnitID& id,
      Graph* graph) {
    std::unique_ptr<CoordinationUnitClass> new_cu =
        std::make_unique<CoordinationUnitClass>(id, graph);
    return static_cast<CoordinationUnitClass*>(
        PassOwnershipToGraph(std::move(new_cu)));
  }

  static const CoordinationUnitClass* FromNodeBase(const NodeBase* cu) {
    DCHECK(cu->id().type == CoordinationUnitClass::Type());
    return static_cast<const CoordinationUnitClass*>(cu);
  }

  static CoordinationUnitClass* FromNodeBase(NodeBase* cu) {
    DCHECK(cu->id().type == CoordinationUnitClass::Type());
    return static_cast<CoordinationUnitClass*>(cu);
  }

  CoordinationUnitInterface(const resource_coordinator::CoordinationUnitID& id,
                            Graph* graph)
      : NodeBase(id, graph), binding_(this) {}

  ~CoordinationUnitInterface() override = default;

  void Bind(MojoRequestClass request) { binding_.Bind(std::move(request)); }

  void GetID(typename MojoInterfaceClass::GetIDCallback callback) override {
    std::move(callback).Run(id_);
  }
  void AddBinding(MojoRequestClass request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  mojo::Binding<MojoInterfaceClass>& binding() { return binding_; }

  static CoordinationUnitClass* GetNodeByID(
      Graph* graph,
      const resource_coordinator::CoordinationUnitID cu_id) {
    DCHECK(cu_id.type == CoordinationUnitClass::Type());
    auto* cu = graph->GetNodeByID(cu_id);
    if (!cu)
      return nullptr;

    CHECK_EQ(cu->id().type, CoordinationUnitClass::Type());
    return static_cast<CoordinationUnitClass*>(cu);
  }

 private:
  mojo::BindingSet<MojoInterfaceClass> bindings_;
  mojo::Binding<MojoInterfaceClass> binding_;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitInterface);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_NODE_BASE_H_
