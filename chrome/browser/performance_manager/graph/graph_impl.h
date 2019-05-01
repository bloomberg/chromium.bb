// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_manager/graph/node_attached_data.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace performance_manager {

class NodeBase;
class GraphObserver;
class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// Represents a graph of the nodes representing a single browser. Maintains a
// set of nodes that can be retrieved in different ways, some indexed. Keeps
// a list of observers that are notified of node addition and removal.
class GraphImpl {
 public:
  using NodeSet = std::unordered_set<NodeBase*>;

  GraphImpl();
  ~GraphImpl();

  void set_ukm_recorder(ukm::UkmRecorder* ukm_recorder) {
    ukm_recorder_ = ukm_recorder;
  }
  ukm::UkmRecorder* ukm_recorder() const { return ukm_recorder_; }

  // Register |observer| on the graph.
  void RegisterObserver(GraphObserver* observer);

  // Unregister |observer| from observing graph changes. Note that this does not
  // unregister |observer| from any nodes it's subscribed to.
  void UnregisterObserver(GraphObserver* observer);

  SystemNodeImpl* FindOrCreateSystemNode();
  std::vector<ProcessNodeImpl*> GetAllProcessNodes();
  std::vector<FrameNodeImpl*> GetAllFrameNodes();
  std::vector<PageNodeImpl*> GetAllPageNodes();
  const NodeSet& nodes() { return nodes_; }

  // Retrieves the process node with PID |pid|, if any.
  ProcessNodeImpl* GetProcessNodeByPid(base::ProcessId pid);

  // Returns true if |node| is in this graph.
  bool NodeInGraph(const NodeBase* node);

  std::vector<GraphObserver*>& observers_for_testing() { return observers_; }

  // Management functions for node owners, any node added to the graph must be
  // removed from the graph before it's deleted.
  void AddNewNode(NodeBase* new_node);
  void RemoveNode(NodeBase* node);

  // A |key| of nullptr counts all instances associated with the |node|. A
  // |node| of null counts all instances associated with the |key|. If both are
  // null then the entire map size is provided.
  size_t GetNodeAttachedDataCountForTesting(NodeBase* node,
                                            const void* key) const;

 private:
  using ProcessByPidMap = std::unordered_map<base::ProcessId, ProcessNodeImpl*>;

  void OnNodeAdded(NodeBase* node);
  void OnBeforeNodeRemoved(NodeBase* node);

  // Returns a new serialization ID.
  friend class NodeBase;
  int64_t GetNextNodeSerializationId();

  // Process PID map for use by ProcessNodeImpl.
  friend class ProcessNodeImpl;
  void BeforeProcessPidChange(ProcessNodeImpl* process,
                              base::ProcessId new_pid);

  template <typename NodeType>
  std::vector<NodeType*> GetAllNodesOfType();

  std::unique_ptr<SystemNodeImpl> system_node_;
  NodeSet nodes_;
  ProcessByPidMap processes_by_pid_;
  std::vector<GraphObserver*> observers_;
  ukm::UkmRecorder* ukm_recorder_ = nullptr;

  // User data storage for the graph.
  friend class NodeAttachedData;
  using NodeAttachedDataKey = std::pair<const NodeBase*, const void*>;
  using NodeAttachedDataMap =
      std::map<NodeAttachedDataKey, std::unique_ptr<NodeAttachedData>>;
  NodeAttachedDataMap node_attached_data_map_;

  // The most recently assigned serialization ID.
  int64_t current_node_serialization_id_ = 0u;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(GraphImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_H_
