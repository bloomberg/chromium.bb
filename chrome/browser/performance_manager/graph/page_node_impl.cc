// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/page_node_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"

namespace performance_manager {

namespace {

constexpr size_t kMaxInterventionIndex = static_cast<size_t>(
    resource_coordinator::mojom::PolicyControlledIntervention::kMaxValue);

size_t ToIndex(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  const size_t kIndex = static_cast<size_t>(intervention);
  DCHECK(kIndex <= kMaxInterventionIndex);
  return kIndex;
}

}  // namespace

PageNodeImpl::PageNodeImpl(Graph* graph)
    : TypedNodeBase(graph),
      visibility_change_time_(ResourceCoordinatorClock::NowTicks()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PageNodeImpl::~PageNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PageNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->GetPageNode());
  DCHECK(NodeInGraph(frame_node));

  // TODO(https://crbug.com/944150): This method is called on navigation
  //     complete, and as such can fire more than once for a given frame in
  //     its lifetime. The |frame_nodes_| set is redundant to the frame tree and
  //     should be removed.
  const bool inserted = frame_nodes_.insert(frame_node).second;
  if (inserted) {

    OnNumFrozenFramesStateChange(
        frame_node->lifecycle_state() ==
                resource_coordinator::mojom::LifecycleState::kFrozen
            ? 1
            : 0);
    MaybeInvalidateInterventionPolicies(frame_node, true /* adding_frame */);
  }
}

void PageNodeImpl::MaybeRemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->GetPageNode());
  DCHECK(NodeInGraph(frame_node));

  size_t removed = frame_nodes_.erase(frame_node);
  if (removed) {
    OnNumFrozenFramesStateChange(
        frame_node->lifecycle_state() ==
                resource_coordinator::mojom::LifecycleState::kFrozen
            ? -1
            : 0);
    MaybeInvalidateInterventionPolicies(frame_node, false /* adding_frame */);
  }
}

void PageNodeImpl::SetIsLoading(bool is_loading) {
  SetPropertyAndNotifyObserversIfChanged(&GraphObserver::OnIsLoadingChanged,
                                         is_loading, this, &is_loading_);
}

void PageNodeImpl::SetIsVisible(bool is_visible) {
  SetPropertyAndNotifyObserversIfChanged(&GraphObserver::OnIsVisibleChanged,
                                         is_visible, this, &is_visible_);
  // The change time needs to be updated after observers are notified, as they
  // use this to determine time passed since the *previous* visibility state
  // change. They can infer the current state change time themselves via
  // NowTicks.
  visibility_change_time_ = ResourceCoordinatorClock::NowTicks();
}

void PageNodeImpl::SetUKMSourceId(int64_t ukm_source_id) {
  SetProperty(resource_coordinator::mojom::PropertyType::kUKMSourceId,
              ukm_source_id);
}

void PageNodeImpl::OnFaviconUpdated() {
  SendEvent(resource_coordinator::mojom::Event::kFaviconUpdated);
}

void PageNodeImpl::OnTitleUpdated() {
  SendEvent(resource_coordinator::mojom::Event::kTitleUpdated);
}

void PageNodeImpl::OnMainFrameNavigationCommitted(
    base::TimeTicks navigation_committed_time,
    int64_t navigation_id,
    const std::string& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  navigation_committed_time_ = navigation_committed_time;
  main_frame_url_ = url;
  navigation_id_ = navigation_id;
  SendEvent(resource_coordinator::mojom::Event::kNavigationCommitted);
}

std::set<ProcessNodeImpl*> PageNodeImpl::GetAssociatedProcessCoordinationUnits()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<ProcessNodeImpl*> process_nodes;

  for (auto* frame_node : frame_nodes_) {
    if (auto* process_node = frame_node->GetProcessNode()) {
      process_nodes.insert(process_node);
    }
  }
  return process_nodes;
}

double PageNodeImpl::GetCPUUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  double cpu_usage = 0.0;

  for (auto* process_node : GetAssociatedProcessCoordinationUnits()) {
    size_t pages_in_process =
        process_node->GetAssociatedPageCoordinationUnits().size();
    DCHECK_LE(1u, pages_in_process);

    int64_t process_cpu_usage = 0;
    if (process_node->GetProperty(
            resource_coordinator::mojom::PropertyType::kCPUUsage,
            &process_cpu_usage)) {
      cpu_usage += static_cast<double>(process_cpu_usage) / pages_in_process;
    }
  }

  return cpu_usage / 1000;
}

base::TimeDelta PageNodeImpl::TimeSinceLastNavigation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (navigation_committed_time_.is_null())
    return base::TimeDelta();
  return ResourceCoordinatorClock::NowTicks() - navigation_committed_time_;
}

base::TimeDelta PageNodeImpl::TimeSinceLastVisibilityChange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ResourceCoordinatorClock::NowTicks() - visibility_change_time_;
}

FrameNodeImpl* PageNodeImpl::GetMainFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* frame_node : frame_nodes_) {
    if (frame_node->IsMainFrame())
      return frame_node;
  }
  return nullptr;
}

void PageNodeImpl::OnFrameLifecycleStateChanged(
    FrameNodeImpl* frame_node,
    resource_coordinator::mojom::LifecycleState old_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ContainsKey(frame_nodes_, frame_node));
  DCHECK_NE(old_state, frame_node->lifecycle_state());

  int delta = 0;
  if (old_state == resource_coordinator::mojom::LifecycleState::kFrozen)
    delta = -1;
  else if (frame_node->lifecycle_state() ==
           resource_coordinator::mojom::LifecycleState::kFrozen)
    delta = 1;
  if (delta != 0)
    OnNumFrozenFramesStateChange(delta);
}

void PageNodeImpl::OnFrameInterventionPolicyChanged(
    FrameNodeImpl* frame,
    resource_coordinator::mojom::PolicyControlledIntervention intervention,
    resource_coordinator::mojom::InterventionPolicy old_policy,
    resource_coordinator::mojom::InterventionPolicy new_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t kIndex = ToIndex(intervention);

  // Invalidate the local policy aggregation for this intervention. It will be
  // recomputed on the next query to GetInterventionPolicy.
  intervention_policy_[kIndex] =
      resource_coordinator::mojom::InterventionPolicy::kUnknown;

  // The first time a frame transitions away from kUnknown for the last policy,
  // then that frame is considered to have checked in. Frames always provide
  // initial policy values in order, ensuring this works.
  if (old_policy == resource_coordinator::mojom::InterventionPolicy::kUnknown &&
      new_policy != resource_coordinator::mojom::InterventionPolicy::kUnknown &&
      intervention == resource_coordinator::mojom::
                          PolicyControlledIntervention::kMaxValue) {
    ++intervention_policy_frames_reported_;
    DCHECK_LE(intervention_policy_frames_reported_, frame_nodes_.size());
  }
}

resource_coordinator::mojom::InterventionPolicy
PageNodeImpl::GetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there are no frames, or they've not all reported, then return kUnknown.
  if (frame_nodes_.empty() ||
      intervention_policy_frames_reported_ != frame_nodes_.size()) {
    return resource_coordinator::mojom::InterventionPolicy::kUnknown;
  }

  // Recompute the policy if it is currently invalid.
  const size_t kIndex = ToIndex(intervention);
  DCHECK_LE(kIndex, kMaxInterventionIndex);
  if (intervention_policy_[kIndex] ==
      resource_coordinator::mojom::InterventionPolicy::kUnknown) {
    RecomputeInterventionPolicy(intervention);
    DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
              intervention_policy_[kIndex]);
  }

  return intervention_policy_[kIndex];
}

void PageNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateAllInterventionPolicies();

  NodeBase::JoinGraph();
}

void PageNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(frame_nodes_.empty());

  NodeBase::LeaveGraph();
}

void PageNodeImpl::SetPageAlmostIdle(bool page_almost_idle) {
  SetPropertyAndNotifyObserversIfChanged(
      &GraphObserver::OnPageAlmostIdleChanged, page_almost_idle, this,
      &page_almost_idle_);
}

void PageNodeImpl::OnEventReceived(resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnPageEventReceived(this, event);
}

void PageNodeImpl::OnPropertyChanged(
    const resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnPagePropertyChanged(this, property_type, value);
}

void PageNodeImpl::OnNumFrozenFramesStateChange(int num_frozen_frames_delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_frozen_frames_ += num_frozen_frames_delta;
  DCHECK_LE(num_frozen_frames_, frame_nodes_.size());

  const int64_t kRunning = static_cast<int64_t>(
      resource_coordinator::mojom::LifecycleState::kRunning);
  const int64_t kFrozen = static_cast<int64_t>(
      resource_coordinator::mojom::LifecycleState::kFrozen);

  // We are interested in knowing when we have transitioned to or from
  // "fully frozen". A page with no frames is considered to be running by
  // default.
  bool was_fully_frozen =
      GetPropertyOrDefault(
          resource_coordinator::mojom::PropertyType::kLifecycleState,
          kRunning) == kFrozen;
  bool is_fully_frozen =
      !frame_nodes_.empty() && num_frozen_frames_ == frame_nodes_.size();
  if (was_fully_frozen == is_fully_frozen)
    return;

  if (is_fully_frozen) {
    // Aggregate the beforeunload handler information from the entire frame
    // tree.
    bool has_nonempty_beforeunload = false;
    for (auto* frame : frame_nodes_) {
      if (frame->has_nonempty_beforeunload()) {
        has_nonempty_beforeunload = true;
        break;
      }
    }
    set_has_nonempty_beforeunload(has_nonempty_beforeunload);
  }

  // TODO(fdoray): Store the lifecycle state as a member on the
  // PageCoordinationUnit rather than as a non-typed property.
  SetProperty(resource_coordinator::mojom::PropertyType::kLifecycleState,
              is_fully_frozen ? kFrozen : kRunning);
}

void PageNodeImpl::InvalidateAllInterventionPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i <= kMaxInterventionIndex; ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;
}

void PageNodeImpl::MaybeInvalidateInterventionPolicies(
    FrameNodeImpl* frame_node,
    bool adding_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ensure that the frame was already added or removed as expected.
  DCHECK(adding_frame == frame_nodes_.count(frame_node));

  // Determine whether or not the frames had all reported prior to this change.
  const size_t prior_frame_count =
      frame_nodes_.size() + (adding_frame ? -1 : 1);
  const bool frames_all_reported_prior =
      prior_frame_count > 0 &&
      intervention_policy_frames_reported_ == prior_frame_count;

  // If the previous state was considered fully reported, then aggregation may
  // have occurred. Adding or removing a frame (even one that is fully reported)
  // needs to invalidate that aggregation. Invalidation could happen on every
  // single frame addition and removal, but only doing this when the previous
  // state was fully reported reduces unnecessary invalidations.
  if (frames_all_reported_prior)
    InvalidateAllInterventionPolicies();

  // Update the reporting frame count.
  const bool frame_reported = frame_node->AreAllInterventionPoliciesSet();
  if (frame_reported)
    intervention_policy_frames_reported_ += adding_frame ? 1 : -1;

  DCHECK_LE(intervention_policy_frames_reported_, frame_nodes_.size());
}

void PageNodeImpl::RecomputeInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t kIndex = ToIndex(intervention);

  // This should never be called with an empty frame tree.
  DCHECK(!frame_nodes_.empty());

  resource_coordinator::mojom::InterventionPolicy policy =
      resource_coordinator::mojom::InterventionPolicy::kDefault;
  for (auto* frame : frame_nodes_) {
    // No frame should have an unknown policy, as aggregation should only be
    // invoked after all frames have checked in.
    DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
              frame->intervention_policy_[kIndex]);

    // If any frame opts out then the whole frame tree opts out, even if other
    // frames have opted in.
    if (frame->intervention_policy_[kIndex] ==
        resource_coordinator::mojom::InterventionPolicy::kOptOut) {
      intervention_policy_[kIndex] =
          resource_coordinator::mojom::InterventionPolicy::kOptOut;
      return;
    }

    // If any frame opts in and none opt out, then the whole tree opts in.
    if (frame->intervention_policy_[kIndex] ==
        resource_coordinator::mojom::InterventionPolicy::kOptIn) {
      policy = resource_coordinator::mojom::InterventionPolicy::kOptIn;
    }
  }

  intervention_policy_[kIndex] = policy;
}

}  // namespace performance_manager
