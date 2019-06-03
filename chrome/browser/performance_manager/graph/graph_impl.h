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
#include "chrome/browser/performance_manager/public/graph/graph.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace performance_manager {

class FrameNodeImpl;
class GraphImplObserver;
class NodeBase;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// Represents a graph of the nodes representing a single browser. Maintains a
// set of nodes that can be retrieved in different ways, some indexed. Keeps
// a list of observers that are notified of node addition and removal.
class GraphImpl : public Graph {
 public:
  // Pure virtual observer interface. Derive from this if you want to manually
  // implement the whole interface, and have the compiler enforce that as new
  // methods are added.
  using Observer = GraphObserver;

  using NodeSet = std::unordered_set<NodeBase*>;

  GraphImpl();
  ~GraphImpl() override;

  // Graph implementation:
  void AddGraphObserver(GraphObserver* observer) override;
  void AddFrameNodeObserver(FrameNodeObserver* observer) override;
  void AddPageNodeObserver(PageNodeObserver* observer) override;
  void AddProcessNodeObserver(ProcessNodeObserver* observer) override;
  void AddSystemNodeObserver(SystemNodeObserver* observer) override;
  void RemoveGraphObserver(GraphObserver* observer) override;
  void RemoveFrameNodeObserver(FrameNodeObserver* observer) override;
  void RemovePageNodeObserver(PageNodeObserver* observer) override;
  void RemoveProcessNodeObserver(ProcessNodeObserver* observer) override;
  void RemoveSystemNodeObserver(SystemNodeObserver* observer) override;
  uintptr_t GetImplType() const override;
  const void* GetImpl() const override;

  // Helper function for safely downcasting to the implementation. This also
  // casts away constness. This will CHECK on an invalid cast.
  static GraphImpl* FromGraph(const Graph* graph);

  void set_ukm_recorder(ukm::UkmRecorder* ukm_recorder) {
    ukm_recorder_ = ukm_recorder;
  }
  ukm::UkmRecorder* ukm_recorder() const { return ukm_recorder_; }

  // Register |observer| on the graph.
  void RegisterObserver(GraphImplObserver* observer);

  // Unregister |observer| from observing graph changes. Note that this does not
  // unregister |observer| from any nodes it's subscribed to.
  void UnregisterObserver(GraphImplObserver* observer);

  SystemNodeImpl* FindOrCreateSystemNode();
  std::vector<ProcessNodeImpl*> GetAllProcessNodes();
  std::vector<FrameNodeImpl*> GetAllFrameNodes();
  std::vector<PageNodeImpl*> GetAllPageNodes();
  const NodeSet& nodes() { return nodes_; }

  // Retrieves the process node with PID |pid|, if any.
  ProcessNodeImpl* GetProcessNodeByPid(base::ProcessId pid);

  // Returns true if |node| is in this graph.
  bool NodeInGraph(const NodeBase* node);

  std::vector<GraphImplObserver*>& observers_for_testing() {
    return observers_;
  }

  // Management functions for node owners, any node added to the graph must be
  // removed from the graph before it's deleted.
  void AddNewNode(NodeBase* new_node);
  void RemoveNode(NodeBase* node);

  // A |key| of nullptr counts all instances associated with the |node|. A
  // |node| of null counts all instances associated with the |key|. If both are
  // null then the entire map size is provided.
  size_t GetNodeAttachedDataCountForTesting(NodeBase* node,
                                            const void* key) const;

  // Allows explicitly invoking SystemNode destruction for testing.
  void ReleaseSystemNodeForTesting() { ReleaseSystemNode(); }

 protected:
  friend class NodeBase;

  // Provides access to per-node-class typed observers. Exposed to nodes via
  // TypedNodeBase.
  template <typename Observer>
  const std::vector<Observer*>& GetObservers() const;
  template <>
  const std::vector<FrameNodeObserver*>& GetObservers() const {
    return frame_node_observers_;
  }
  template <>
  const std::vector<PageNodeObserver*>& GetObservers() const {
    return page_node_observers_;
  }
  template <>
  const std::vector<ProcessNodeObserver*>& GetObservers() const {
    return process_node_observers_;
  }
  template <>
  const std::vector<SystemNodeObserver*>& GetObservers() const {
    return system_node_observers_;
  }

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

  void ReleaseSystemNode();

  std::unique_ptr<SystemNodeImpl> system_node_;
  NodeSet nodes_;
  ProcessByPidMap processes_by_pid_;
  std::vector<GraphImplObserver*> observers_;
  ukm::UkmRecorder* ukm_recorder_ = nullptr;

  // Typed observers.
  // TODO(chrisha): We should wrap these containers in something that catches
  // invalid reentrant usage in DCHECK builds.
  std::vector<GraphObserver*> graph_observers_;
  std::vector<FrameNodeObserver*> frame_node_observers_;
  std::vector<PageNodeObserver*> page_node_observers_;
  std::vector<ProcessNodeObserver*> process_node_observers_;
  std::vector<SystemNodeObserver*> system_node_observers_;

  // User data storage for the graph.
  friend class NodeAttachedDataMapHelper;
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
