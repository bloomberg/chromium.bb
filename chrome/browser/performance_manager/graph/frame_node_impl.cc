// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"

namespace performance_manager {

FrameNodeImpl::FrameNodeImpl(
    const resource_coordinator::CoordinationUnitID& id,
    Graph* graph,
    std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref)
    : CoordinationUnitInterface(id, graph, std::move(keepalive_ref)),
      parent_frame_coordination_unit_(nullptr),
      page_coordination_unit_(nullptr),
      process_coordination_unit_(nullptr) {
  for (size_t i = 0; i < base::size(intervention_policy_); ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;
}

FrameNodeImpl::~FrameNodeImpl() {
  if (parent_frame_coordination_unit_)
    parent_frame_coordination_unit_->RemoveChildFrame(this);
  if (page_coordination_unit_)
    page_coordination_unit_->RemoveFrame(this);
  if (process_coordination_unit_)
    process_coordination_unit_->RemoveFrame(this);
  for (auto* child_frame : child_frame_coordination_units_)
    child_frame->RemoveParentFrame(this);
}

void FrameNodeImpl::SetProcess(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  ProcessNodeImpl* process_cu = ProcessNodeImpl::GetNodeByID(graph_, cu_id);
  if (!process_cu)
    return;
  DCHECK(!process_coordination_unit_);
  process_coordination_unit_ = process_cu;
  process_cu->AddFrame(this);
}

void FrameNodeImpl::AddChildFrame(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK(cu_id != id());
  FrameNodeImpl* frame_cu = FrameNodeImpl::GetNodeByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (HasFrameNodeInAncestors(frame_cu) ||
      frame_cu->HasFrameNodeInDescendants(this)) {
    DCHECK(false) << "Cyclic reference in frame coordination units detected!";
    return;
  }
  if (AddChildFrame(frame_cu)) {
    frame_cu->AddParentFrame(this);
  }
}

void FrameNodeImpl::RemoveChildFrame(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK(cu_id != id());
  FrameNodeImpl* frame_cu = FrameNodeImpl::GetNodeByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (RemoveChildFrame(frame_cu)) {
    frame_cu->RemoveParentFrame(this);
  }
}

void FrameNodeImpl::SetNetworkAlmostIdle(bool idle) {
  SetProperty(resource_coordinator::mojom::PropertyType::kNetworkAlmostIdle,
              idle);
}

void FrameNodeImpl::SetLifecycleState(
    resource_coordinator::mojom::LifecycleState state) {
  if (state == lifecycle_state_)
    return;

  resource_coordinator::mojom::LifecycleState old_state = lifecycle_state_;
  lifecycle_state_ = state;

  // Notify parents of this change.
  if (process_coordination_unit_)
    process_coordination_unit_->OnFrameLifecycleStateChanged(this, old_state);
  if (page_coordination_unit_)
    page_coordination_unit_->OnFrameLifecycleStateChanged(this, old_state);
}

void FrameNodeImpl::SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) {
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void FrameNodeImpl::SetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention,
    resource_coordinator::mojom::InterventionPolicy policy) {
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
  if (auto* page_cu = GetPageNode()) {
    page_cu->OnFrameInterventionPolicyChanged(this, intervention, old_policy,
                                              policy);
  }
}

void FrameNodeImpl::OnNonPersistentNotificationCreated() {
  SendEvent(
      resource_coordinator::mojom::Event::kNonPersistentNotificationCreated);
}

FrameNodeImpl* FrameNodeImpl::GetParentFrameNode() const {
  return parent_frame_coordination_unit_;
}

PageNodeImpl* FrameNodeImpl::GetPageNode() const {
  return page_coordination_unit_;
}

ProcessNodeImpl* FrameNodeImpl::GetProcessNode() const {
  return process_coordination_unit_;
}

bool FrameNodeImpl::IsMainFrame() const {
  return !parent_frame_coordination_unit_;
}

bool FrameNodeImpl::AreAllInterventionPoliciesSet() const {
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
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    SetInterventionPolicy(
        static_cast<resource_coordinator::mojom::PolicyControlledIntervention>(
            i),
        policy);
  }
}

void FrameNodeImpl::OnEventReceived(resource_coordinator::mojom::Event event) {
  for (auto& observer : observers())
    observer.OnFrameEventReceived(this, event);
}

void FrameNodeImpl::OnPropertyChanged(
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers())
    observer.OnFramePropertyChanged(this, property_type, value);
}

bool FrameNodeImpl::HasFrameNodeInAncestors(FrameNodeImpl* frame_cu) const {
  if (parent_frame_coordination_unit_ == frame_cu ||
      (parent_frame_coordination_unit_ &&
       parent_frame_coordination_unit_->HasFrameNodeInAncestors(frame_cu))) {
    return true;
  }
  return false;
}

bool FrameNodeImpl::HasFrameNodeInDescendants(FrameNodeImpl* frame_cu) const {
  for (FrameNodeImpl* child : child_frame_coordination_units_) {
    if (child == frame_cu || child->HasFrameNodeInDescendants(frame_cu)) {
      return true;
    }
  }
  return false;
}

void FrameNodeImpl::AddParentFrame(FrameNodeImpl* parent_frame_cu) {
  parent_frame_coordination_unit_ = parent_frame_cu;
}

bool FrameNodeImpl::AddChildFrame(FrameNodeImpl* child_frame_cu) {
  return child_frame_coordination_units_.count(child_frame_cu)
             ? false
             : child_frame_coordination_units_.insert(child_frame_cu).second;
}

void FrameNodeImpl::RemoveParentFrame(FrameNodeImpl* parent_frame_cu) {
  DCHECK(parent_frame_coordination_unit_ == parent_frame_cu);
  parent_frame_coordination_unit_ = nullptr;
}

bool FrameNodeImpl::RemoveChildFrame(FrameNodeImpl* child_frame_cu) {
  return child_frame_coordination_units_.erase(child_frame_cu) > 0;
}

void FrameNodeImpl::AddPageNode(PageNodeImpl* page_coordination_unit) {
  DCHECK(!page_coordination_unit_);
  page_coordination_unit_ = page_coordination_unit;
}

void FrameNodeImpl::RemovePageNode(PageNodeImpl* page_coordination_unit) {
  DCHECK(page_coordination_unit == page_coordination_unit_);
  page_coordination_unit_ = nullptr;
}

void FrameNodeImpl::RemoveProcessNode(
    ProcessNodeImpl* process_coordination_unit) {
  DCHECK(process_coordination_unit == process_coordination_unit_);
  process_coordination_unit_ = nullptr;
}

}  // namespace performance_manager
