// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"

namespace performance_manager {

FrameNodeImpl::FrameNodeImpl(const resource_coordinator::CoordinationUnitID& id,
                             Graph* graph)
    : CoordinationUnitInterface(id, graph),
      parent_frame_node_(nullptr),
      page_node_(nullptr),
      process_node_(nullptr) {
  for (size_t i = 0; i < base::size(intervention_policy_); ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FrameNodeImpl::~FrameNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_frame_node_)
    parent_frame_node_->RemoveChildFrame(this);
  if (page_node_)
    page_node_->RemoveFrameImpl(this);
  if (process_node_)
    process_node_->RemoveFrame(this);
  for (auto* child_frame : child_frame_nodes_)
    child_frame->RemoveParentFrame(this);
}

void FrameNodeImpl::SetProcess(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ProcessNodeImpl* process_node = ProcessNodeImpl::GetNodeByID(graph_, cu_id);
  if (!process_node)
    return;
  DCHECK(!process_node_);
  process_node_ = process_node;
  process_node->AddFrame(this);
}

void FrameNodeImpl::AddChildFrame(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cu_id != id());
  FrameNodeImpl* frame_node = FrameNodeImpl::GetNodeByID(graph_, cu_id);
  if (!frame_node)
    return;
  if (HasFrameNodeInAncestors(frame_node) ||
      frame_node->HasFrameNodeInDescendants(this)) {
    DCHECK(false) << "Cyclic reference in frame coordination units detected!";
    return;
  }
  if (AddChildFrameImpl(frame_node)) {
    frame_node->AddParentFrame(this);
  }
}

void FrameNodeImpl::RemoveChildFrame(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cu_id != id());
  FrameNodeImpl* frame_node = FrameNodeImpl::GetNodeByID(graph_, cu_id);
  if (!frame_node)
    return;
  if (RemoveChildFrame(frame_node)) {
    frame_node->RemoveParentFrame(this);
  }
}

void FrameNodeImpl::SetNetworkAlmostIdle(bool network_almost_idle) {
  SetPropertyAndNotifyObservers(&GraphObserver::OnNetworkAlmostIdleChanged,
                                network_almost_idle, this,
                                &network_almost_idle_);
}

void FrameNodeImpl::SetLifecycleState(
    resource_coordinator::mojom::LifecycleState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state == lifecycle_state_)
    return;

  resource_coordinator::mojom::LifecycleState old_state = lifecycle_state_;
  lifecycle_state_ = state;

  // Notify parents of this change.
  if (process_node_)
    process_node_->OnFrameLifecycleStateChanged(this, old_state);
  if (page_node_)
    page_node_->OnFrameLifecycleStateChanged(this, old_state);
}

void FrameNodeImpl::SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void FrameNodeImpl::SetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention,
    resource_coordinator::mojom::InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t i = static_cast<size_t>(intervention);
  DCHECK_LT(i, base::size(intervention_policy_));

  // This can only be called to set a policy, but not to revert a policy to the
  // unset state.
  DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown, policy);

  // We expect intervention policies to be initially set in order, and rely on
  // that as a synchronization primitive. Ensure this is the case.
  DCHECK(i == 0 ||
         intervention_policy_[i - 1] !=
             resource_coordinator::mojom::InterventionPolicy::kUnknown);

  if (policy == intervention_policy_[i])
    return;
  // Only notify of actual changes.
  resource_coordinator::mojom::InterventionPolicy old_policy =
      intervention_policy_[i];
  intervention_policy_[i] = policy;
  if (auto* page_node = GetPageNode()) {
    page_node->OnFrameInterventionPolicyChanged(this, intervention, old_policy,
                                                policy);
  }
}

void FrameNodeImpl::OnNonPersistentNotificationCreated() {
  SendEvent(
      resource_coordinator::mojom::Event::kNonPersistentNotificationCreated);
}

FrameNodeImpl* FrameNodeImpl::GetParentFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent_frame_node_;
}

PageNodeImpl* FrameNodeImpl::GetPageNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_node_;
}

ProcessNodeImpl* FrameNodeImpl::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node_;
}

bool FrameNodeImpl::IsMainFrame() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !parent_frame_node_;
}

bool FrameNodeImpl::AreAllInterventionPoliciesSet() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The convention is that policies are first set en masse, in order. So if
  // the last policy is set then they are all considered to be set. Check this
  // in DEBUG builds.
#if DCHECK_IS_ON()
  bool seen_unset_policy = false;
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    if (!seen_unset_policy) {
      seen_unset_policy =
          intervention_policy_[i] !=
          resource_coordinator::mojom::InterventionPolicy::kUnknown;
    } else {
      // Once a first unset policy is seen, all subsequent policies must be
      // unset.
      DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
                intervention_policy_[i]);
    }
  }
#endif

  return intervention_policy_[base::size(intervention_policy_) - 1] !=
         resource_coordinator::mojom::InterventionPolicy::kUnknown;
}  // namespace performance_manager

void FrameNodeImpl::SetAllInterventionPoliciesForTesting(
    resource_coordinator::mojom::InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    SetInterventionPolicy(
        static_cast<resource_coordinator::mojom::PolicyControlledIntervention>(
            i),
        policy);
  }
}

void FrameNodeImpl::OnEventReceived(resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnFrameEventReceived(this, event);
}

void FrameNodeImpl::OnPropertyChanged(
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnFramePropertyChanged(this, property_type, value);
}

bool FrameNodeImpl::HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_frame_node_ == frame_node ||
      (parent_frame_node_ &&
       parent_frame_node_->HasFrameNodeInAncestors(frame_node))) {
    return true;
  }
  return false;
}

bool FrameNodeImpl::HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (FrameNodeImpl* child : child_frame_nodes_) {
    if (child == frame_node || child->HasFrameNodeInDescendants(frame_node)) {
      return true;
    }
  }
  return false;
}

void FrameNodeImpl::AddParentFrame(FrameNodeImpl* parent_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parent_frame_node_ = parent_frame_node;
}

bool FrameNodeImpl::AddChildFrameImpl(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return child_frame_nodes_.count(child_frame_node)
             ? false
             : child_frame_nodes_.insert(child_frame_node).second;
}

void FrameNodeImpl::RemoveParentFrame(FrameNodeImpl* parent_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(parent_frame_node_ == parent_frame_node);
  parent_frame_node_ = nullptr;
}

bool FrameNodeImpl::RemoveChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return child_frame_nodes_.erase(child_frame_node) > 0;
}

void FrameNodeImpl::AddPageNode(PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!page_node_);
  page_node_ = page_node;
}

void FrameNodeImpl::RemovePageNode(PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_node == page_node_);
  page_node_ = nullptr;
}

void FrameNodeImpl::RemoveProcessNode(ProcessNodeImpl* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process_node == process_node_);
  process_node_ = nullptr;
}

}  // namespace performance_manager
