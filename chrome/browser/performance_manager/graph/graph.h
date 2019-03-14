// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "chrome/browser/performance_manager/graph/node_attached_data.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"

namespace service_manager {
template <typename... BinderArgs>
class BinderRegistryWithArgs;
struct BindSourceInfo;
}  // namespace service_manager

namespace performance_manager {

class NodeBase;
class GraphObserver;
class GraphNodeProviderImpl;
class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// The Graph represents a graph of the coordination units
// representing a single system. It vends out new instances of coordination
// units and indexes them by ID. It also fires the creation and pre-destruction
// notifications for all coordination units.
class Graph {
 public:
  Graph();
  ~Graph();

  void set_ukm_recorder(ukm::UkmRecorder* ukm_recorder) {
    ukm_recorder_ = ukm_recorder;
  }
  ukm::UkmRecorder* ukm_recorder() const { return ukm_recorder_; }

  void OnStart(service_manager::BinderRegistryWithArgs<
               const service_manager::BindSourceInfo&>* registry);
  void RegisterObserver(std::unique_ptr<GraphObserver> observer);
  void OnNodeCreated(NodeBase* coordination_unit);
  void OnBeforeNodeDestroyed(NodeBase* coordination_unit);

  FrameNodeImpl* CreateFrameNode(
      const resource_coordinator::CoordinationUnitID& id);
  PageNodeImpl* CreatePageNode(
      const resource_coordinator::CoordinationUnitID& id);
  ProcessNodeImpl* CreateProcessNode(
      const resource_coordinator::CoordinationUnitID& id);
  SystemNodeImpl* FindOrCreateSystemNode();

  std::vector<ProcessNodeImpl*> GetAllProcessNodes();
  std::vector<FrameNodeImpl*> GetAllFrameNodes();
  std::vector<PageNodeImpl*> GetAllPageNodes();

  // Retrieves the process CU with PID |pid|, if any.
  ProcessNodeImpl* GetProcessNodeByPid(base::ProcessId pid);
  NodeBase* GetNodeByID(const resource_coordinator::CoordinationUnitID cu_id);

  std::vector<std::unique_ptr<GraphObserver>>& observers_for_testing() {
    return observers_;
  }

  // A |key| of nullptr counts all instances associated with the |node|. A
  // |node| of null counts all instances associated with the |key|. If both are
  // null then the entire map size is provided.
  size_t GetNodeAttachedDataCountForTesting(NodeBase* node,
                                            const void* key) const;

 private:
  using CUIDMap = std::unordered_map<resource_coordinator::CoordinationUnitID,
                                     std::unique_ptr<NodeBase>>;
  using ProcessByPidMap = std::unordered_map<base::ProcessId, ProcessNodeImpl*>;

  // Lifetime management functions for NodeBase.
  friend class NodeBase;
  NodeBase* AddNewNode(std::unique_ptr<NodeBase> new_cu);
  void DestroyNode(NodeBase* cu);

  // Process PID map for use by ProcessNodeImpl.
  friend class ProcessNodeImpl;
  void BeforeProcessPidChange(ProcessNodeImpl* process,
                              base::ProcessId new_pid);

  template <typename CUType>
  std::vector<CUType*> GetAllNodesOfType();

  resource_coordinator::CoordinationUnitID system_coordination_unit_id_;
  CUIDMap coordination_units_;
  ProcessByPidMap processes_by_pid_;
  std::vector<std::unique_ptr<GraphObserver>> observers_;
  ukm::UkmRecorder* ukm_recorder_ = nullptr;
  std::unique_ptr<GraphNodeProviderImpl> provider_;

  // User data storage for the graph.
  friend class NodeAttachedData;
  using NodeAttachedDataKey = std::pair<const NodeBase*, const void*>;
  using NodeAttachedDataMap =
      std::map<NodeAttachedDataKey, std::unique_ptr<NodeAttachedData>>;
  NodeAttachedDataMap node_attached_data_map_;

  static void Create();

  DISALLOW_COPY_AND_ASSIGN(Graph);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_H_
