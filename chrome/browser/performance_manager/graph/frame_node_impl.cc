// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"

namespace performance_manager {

FrameNodeImpl::FrameNodeImpl(Graph* graph,
                             PageNodeImpl* page_node,
                             FrameNodeImpl* parent_frame_node)
    : CoordinationUnitInterface(graph),
      parent_frame_node_(parent_frame_node),
      page_node_(page_node),
      process_node_(nullptr) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FrameNodeImpl::~FrameNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FrameNodeImpl::SetProcess(ProcessNodeImpl* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(NodeInGraph(process_node));
  DCHECK(!process_node_);
  process_node_ = process_node;
  process_node->AddFrame(this);
}

void FrameNodeImpl::AddChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->GetParentFrameNode());
  DCHECK_NE(this, child_frame_node);
  DCHECK(NodeInGraph(child_frame_node));
  DCHECK(!HasFrameNodeInAncestors(child_frame_node) &&
         !child_frame_node->HasFrameNodeInDescendants(this));

  bool inserted = child_frame_nodes_.insert(child_frame_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->GetParentFrameNode());
  DCHECK_NE(this, child_frame_node);
  DCHECK(NodeInGraph(child_frame_node));

  size_t removed = child_frame_nodes_.erase(child_frame_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::SetNetworkAlmostIdle(bool network_almost_idle) {
  network_almost_idle_.SetAndMaybeNotify(this, network_almost_idle);
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

void FrameNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < base::size(intervention_policy_); ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;

  // Hook up the frame hierarchy.
  if (parent_frame_node_)
    parent_frame_node_->AddChildFrame(this);

  // And join the page.
  // TODO(siggi): Only do this for the main frame and retire the frame set.
  page_node_->AddFrame(this);

  NodeBase::JoinGraph();
}

void FrameNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  DCHECK(child_frame_nodes_.empty());

  // Leave the page.
  page_node_->RemoveFrame(this);

  // Leave the frame hierarchy.
  if (parent_frame_node_)
    parent_frame_node_->RemoveChildFrame(this);

  if (process_node_)
    process_node_->RemoveFrame(this);
}

void FrameNodeImpl::OnEventReceived(resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnFrameEventReceived(this, event);
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

void FrameNodeImpl::RemoveProcessNode(ProcessNodeImpl* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process_node == process_node_);
  process_node_ = nullptr;
}

}  // namespace performance_manager
