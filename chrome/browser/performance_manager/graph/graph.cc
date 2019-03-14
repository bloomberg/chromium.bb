// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_node_provider_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace performance_manager {

Graph::Graph()
    : system_coordination_unit_id_(
          resource_coordinator::CoordinationUnitType::kSystem,
          resource_coordinator::CoordinationUnitID::RANDOM_ID) {}

Graph::~Graph() {
  // Because the graph has ownership of the CUs, and because the process CUs
  // unregister on destruction, there is reentrancy to this class on
  // destruction. The order of operations here is optimized to minimize the work
  // done on destruction, as well as to make sure the cleanup is independent of
  // the declaration order of member variables.

  // Kill all the observers first.
  observers_.clear();
  // Then clear up the CUs to ensure this happens before the PID map is
  // destructed.
  coordination_units_.clear();

  DCHECK_EQ(0u, processes_by_pid_.size());
}

void Graph::OnStart(service_manager::BinderRegistryWithArgs<
                    const service_manager::BindSourceInfo&>* registry) {
  // Create the singleton CoordinationUnitProvider.
  provider_ = std::make_unique<GraphNodeProviderImpl>(this);
  registry->AddInterface(base::BindRepeating(
      &GraphNodeProviderImpl::Bind, base::Unretained(provider_.get())));
}

void Graph::RegisterObserver(std::unique_ptr<GraphObserver> observer) {
  observer->set_coordination_unit_graph(this);
  observers_.push_back(std::move(observer));
}

void Graph::OnNodeCreated(NodeBase* coordination_unit) {
  for (auto& observer : observers_) {
    if (observer->ShouldObserve(coordination_unit)) {
      coordination_unit->AddObserver(observer.get());
      observer->OnNodeCreated(coordination_unit);
    }
  }
}

void Graph::OnBeforeNodeDestroyed(NodeBase* coordination_unit) {
  coordination_unit->BeforeDestroyed();
}

FrameNodeImpl* Graph::CreateFrameNode(
    const resource_coordinator::CoordinationUnitID& id) {
  return FrameNodeImpl::Create(id, this);
}

PageNodeImpl* Graph::CreatePageNode(
    const resource_coordinator::CoordinationUnitID& id) {
  return PageNodeImpl::Create(id, this);
}

ProcessNodeImpl* Graph::CreateProcessNode(
    const resource_coordinator::CoordinationUnitID& id) {
  return ProcessNodeImpl::Create(id, this);
}

SystemNodeImpl* Graph::FindOrCreateSystemNode() {
  NodeBase* system_cu = GetNodeByID(system_coordination_unit_id_);
  if (system_cu)
    return SystemNodeImpl::FromNodeBase(system_cu);

  // Create the singleton SystemCU instance. Ownership is taken by the graph.
  return SystemNodeImpl::Create(system_coordination_unit_id_, this);
}

NodeBase* Graph::GetNodeByID(
    const resource_coordinator::CoordinationUnitID cu_id) {
  const auto& it = coordination_units_.find(cu_id);
  if (it == coordination_units_.end())
    return nullptr;
  return it->second.get();
}

ProcessNodeImpl* Graph::GetProcessNodeByPid(base::ProcessId pid) {
  auto it = processes_by_pid_.find(pid);
  if (it == processes_by_pid_.end())
    return nullptr;

  return ProcessNodeImpl::FromNodeBase(it->second);
}

std::vector<ProcessNodeImpl*> Graph::GetAllProcessNodes() {
  return GetAllNodesOfType<ProcessNodeImpl>();
}

std::vector<FrameNodeImpl*> Graph::GetAllFrameNodes() {
  return GetAllNodesOfType<FrameNodeImpl>();
}

std::vector<PageNodeImpl*> Graph::GetAllPageNodes() {
  return GetAllNodesOfType<PageNodeImpl>();
}

size_t Graph::GetNodeAttachedDataCountForTesting(NodeBase* node,
                                                 const void* key) const {
  if (!node && !key)
    return node_attached_data_map_.size();

  size_t count = 0;
  for (auto& node_data : node_attached_data_map_) {
    if (node && node_data.first.first != node)
      continue;
    if (key && node_data.first.second != key)
      continue;
    ++count;
  }

  return count;
}

NodeBase* Graph::AddNewNode(std::unique_ptr<NodeBase> new_cu) {
  auto it = coordination_units_.emplace(new_cu->id(), std::move(new_cu));
  DCHECK(it.second);  // Inserted successfully

  NodeBase* added_cu = it.first->second.get();
  OnNodeCreated(added_cu);

  return added_cu;
}

void Graph::DestroyNode(NodeBase* cu) {
  OnBeforeNodeDestroyed(cu);

  // Remove any node attached data affiliated with this node.
  auto lower = node_attached_data_map_.lower_bound(std::make_pair(cu, nullptr));
  auto upper =
      node_attached_data_map_.lower_bound(std::make_pair(cu + 1, nullptr));
  node_attached_data_map_.erase(lower, upper);

  // Before removing the node itself.
  size_t erased = coordination_units_.erase(cu->id());
  DCHECK_EQ(1u, erased);
}

void Graph::BeforeProcessPidChange(ProcessNodeImpl* process,
                                   base::ProcessId new_pid) {
  // On Windows, PIDs are aggressively reused, and because not all process
  // creation/death notifications are synchronized, it's possible for more than
  // one CU to have the same PID. To handle this, the second and subsequent
  // registration override earlier registrations, while unregistration will only
  // unregister the current holder of the PID.
  if (process->process_id() != base::kNullProcessId) {
    auto it = processes_by_pid_.find(process->process_id());
    if (it != processes_by_pid_.end() && it->second == process)
      processes_by_pid_.erase(it);
  }
  if (new_pid != base::kNullProcessId)
    processes_by_pid_[new_pid] = process;
}

template <typename CUType>
std::vector<CUType*> Graph::GetAllNodesOfType() {
  const auto type = CUType::Type();
  std::vector<CUType*> ret;
  for (const auto& el : coordination_units_) {
    if (el.first.type == type)
      ret.push_back(CUType::FromNodeBase(el.second.get()));
  }
  return ret;
}

}  // namespace performance_manager
