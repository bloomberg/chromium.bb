// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/performance_manager/graph/graph.h"
#include "chrome/browser/performance_manager/graph/graph_introspector_impl.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"
#include "services/resource_coordinator/public/mojom/coordination_unit_provider.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ukm {
class MojoUkmRecorder;
}  // namespace ukm

namespace performance_manager {

class PageNodeImpl;

// The performance manager is a rendezvous point for binding to performance
// manager interfaces.
// TODO(https://crbug.com/910288): Refactor this along with the
// {Frame|Page|Process|System}ResourceCoordinator classes.
class PerformanceManager {
 public:
  ~PerformanceManager();

  // Retrieves the currently registered instance.
  // The caller needs to ensure that the lifetime of the registered instance
  // exceeds the use of this function and the retrieved pointer.
  // This function can be called from any sequence with those caveats.
  static PerformanceManager* GetInstance();

  // Creates, initializes and registers an instance.
  static std::unique_ptr<PerformanceManager> Create();

  // Unregisters |instance| if it's currently registered and arranges for its
  // deletion on its sequence.
  static void Destroy(std::unique_ptr<PerformanceManager> instance);

  // Forwards the binding request to the implementation class.
  template <typename Interface>
  void BindInterface(mojo::InterfaceRequest<Interface> request);

  // Dispatches a measurement batch to the SystemNode on the performance
  // sequence. This is a temporary method to support the RenderProcessProbe,
  // which will soon go away as the performance measurement moves to the
  // performance sequence.
  void DistributeMeasurementBatch(
      resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr batch);

  // Creates a new node of the requested type and adds it to the graph.
  // May be called from any sequence.
  std::unique_ptr<FrameNodeImpl> CreateFrameNode();
  std::unique_ptr<PageNodeImpl> CreatePageNode();
  std::unique_ptr<ProcessNodeImpl> CreateProcessNode();

  // Destroys a node returned from the creation functions above.
  // May be called from any sequence.
  template <typename NodeType>
  void DeleteNode(std::unique_ptr<NodeType> node);

  // TODO(siggi): Can this be hidden away?
  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

 private:
  using InterfaceRegistry = service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>;

  PerformanceManager();

  void PostBindInterface(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle message_pipe);

  template <typename NodeType>
  std::unique_ptr<NodeType> CreateNodeImpl();

  void PostDeleteNode(std::unique_ptr<NodeBase> node);
  void DeleteNodeImpl(std::unique_ptr<NodeBase> node);

  void OnStart();
  void OnStartImpl(std::unique_ptr<service_manager::Connector> connector);
  void BindInterfaceImpl(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle message_pipe);
  void DistributeMeasurementBatchImpl(
      resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr batch);

  void BindWebUIGraphDump(
      resource_coordinator::mojom::WebUIGraphDumpRequest request,
      const service_manager::BindSourceInfo& source_info);
  void OnGraphDumpConnectionError(WebUIGraphDumpImpl* graph_dump);

  InterfaceRegistry interface_registry_;

  // The performance task runner.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Graph graph_;
  CoordinationUnitIntrospectorImpl introspector_;

  // Provided to |graph_|.
  // TODO(siggi): This no longer needs to go through mojo.
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // Current graph dump instances.
  std::vector<std::unique_ptr<WebUIGraphDumpImpl>> graph_dumps_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PerformanceManager);
};

template <typename Interface>
void PerformanceManager::BindInterface(
    mojo::InterfaceRequest<Interface> request) {
  PostBindInterface(Interface::Name_, request.PassMessagePipe());
}

template <typename NodeType>
void PerformanceManager::DeleteNode(std::unique_ptr<NodeType> node) {
  PostDeleteNode(std::move(node));
}

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_H_
