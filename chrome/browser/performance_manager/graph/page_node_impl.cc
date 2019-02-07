// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/page_node_impl.h"

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

PageNodeImpl::PageNodeImpl(
    const resource_coordinator::CoordinationUnitID& id,
    Graph* graph,
    std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref)
    : CoordinationUnitInterface(id, graph, std::move(keepalive_ref)) {
  InvalidateAllInterventionPolicies();
}

PageNodeImpl::~PageNodeImpl() {
  for (auto* child_frame : frame_coordination_units_)
    child_frame->RemovePageNode(this);
}

void PageNodeImpl::AddFrame(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK(cu_id.type == resource_coordinator::CoordinationUnitType::kFrame);
  FrameNodeImpl* frame_cu = FrameNodeImpl::GetNodeByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (AddFrame(frame_cu))
    frame_cu->AddPageNode(this);
}

void PageNodeImpl::RemoveFrame(
    const resource_coordinator::CoordinationUnitID& cu_id) {
  DCHECK(cu_id != id());
  FrameNodeImpl* frame_cu = FrameNodeImpl::GetNodeByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (RemoveFrame(frame_cu))
    frame_cu->RemovePageNode(this);
}

void PageNodeImpl::SetIsLoading(bool is_loading) {
  SetProperty(resource_coordinator::mojom::PropertyType::kIsLoading,
              is_loading);
}

void PageNodeImpl::SetVisibility(bool visible) {
  SetProperty(resource_coordinator::mojom::PropertyType::kVisible, visible);
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
  navigation_committed_time_ = navigation_committed_time;
  main_frame_url_ = url;
  navigation_id_ = navigation_id;
  SendEvent(resource_coordinator::mojom::Event::kNavigationCommitted);
}

std::set<ProcessNodeImpl*> PageNodeImpl::GetAssociatedProcessCoordinationUnits()
    const {
  std::set<ProcessNodeImpl*> process_cus;

  for (auto* frame_cu : frame_coordination_units_) {
    if (auto* process_cu = frame_cu->GetProcessNode()) {
      process_cus.insert(process_cu);
    }
  }
  return process_cus;
}

bool PageNodeImpl::IsVisible() const {
  int64_t is_visible = 0;
  bool has_property = GetProperty(
      resource_coordinator::mojom::PropertyType::kVisible, &is_visible);
  DCHECK(has_property && (is_visible == 0 || is_visible == 1));
  return is_visible;
}

double PageNodeImpl::GetCPUUsage() const {
  double cpu_usage = 0.0;

  for (auto* process_cu : GetAssociatedProcessCoordinationUnits()) {
    size_t pages_in_process =
        process_cu->GetAssociatedPageCoordinationUnits().size();
    DCHECK_LE(1u, pages_in_process);

    int64_t process_cpu_usage = 0;
    if (process_cu->GetProperty(
            resource_coordinator::mojom::PropertyType::kCPUUsage,
            &process_cpu_usage)) {
      cpu_usage += static_cast<double>(process_cpu_usage) / pages_in_process;
    }
  }

  return cpu_usage / 1000;
}

bool PageNodeImpl::GetExpectedTaskQueueingDuration(int64_t* output) {
  // Calculate the EQT for the process of the main frame only because
  // the smoothness of the main frame may affect the users the most.
  FrameNodeImpl* main_frame_cu = GetMainFrameNode();
  if (!main_frame_cu)
    return false;
  auto* process_cu = main_frame_cu->GetProcessNode();
  if (!process_cu)
    return false;
  return process_cu->GetProperty(
      resource_coordinator::mojom::PropertyType::kExpectedTaskQueueingDuration,
      output);
}

base::TimeDelta PageNodeImpl::TimeSinceLastNavigation() const {
  if (navigation_committed_time_.is_null())
    return base::TimeDelta();
  return ResourceCoordinatorClock::NowTicks() - navigation_committed_time_;
}

base::TimeDelta PageNodeImpl::TimeSinceLastVisibilityChange() const {
  return ResourceCoordinatorClock::NowTicks() - visibility_change_time_;
}

FrameNodeImpl* PageNodeImpl::GetMainFrameNode() const {
  for (auto* frame_cu : frame_coordination_units_) {
    if (frame_cu->IsMainFrame())
      return frame_cu;
  }
  return nullptr;
}

void PageNodeImpl::OnFrameLifecycleStateChanged(
    FrameNodeImpl* frame_cu,
    resource_coordinator::mojom::LifecycleState old_state) {
  DCHECK(base::ContainsKey(frame_coordination_units_, frame_cu));
  DCHECK_NE(old_state, frame_cu->lifecycle_state());

  int delta = 0;
  if (old_state == resource_coordinator::mojom::LifecycleState::kFrozen)
    delta = -1;
  else if (frame_cu->lifecycle_state() ==
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
    DCHECK_LE(intervention_policy_frames_reported_,
              frame_coordination_units_.size());
  }
}

resource_coordinator::mojom::InterventionPolicy
PageNodeImpl::GetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  // If there are no frames, or they've not all reported, then return kUnknown.
  if (frame_coordination_units_.empty() ||
      intervention_policy_frames_reported_ !=
          frame_coordination_units_.size()) {
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

void PageNodeImpl::OnEventReceived(resource_coordinator::mojom::Event event) {
  for (auto& observer : observers())
    observer.OnPageEventReceived(this, event);
}

void PageNodeImpl::OnPropertyChanged(
    const resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == resource_coordinator::mojom::PropertyType::kVisible)
    visibility_change_time_ = ResourceCoordinatorClock::NowTicks();
  for (auto& observer : observers())
    observer.OnPagePropertyChanged(this, property_type, value);
}

bool PageNodeImpl::AddFrame(FrameNodeImpl* frame_cu) {
  const bool inserted = frame_coordination_units_.insert(frame_cu).second;
  if (inserted) {
    OnNumFrozenFramesStateChange(
        frame_cu->lifecycle_state() ==
                resource_coordinator::mojom::LifecycleState::kFrozen
            ? 1
            : 0);
    MaybeInvalidateInterventionPolicies(frame_cu, true /* adding_frame */);
  }
  return inserted;
}

bool PageNodeImpl::RemoveFrame(FrameNodeImpl* frame_cu) {
  bool removed = frame_coordination_units_.erase(frame_cu) > 0;
  if (removed) {
    OnNumFrozenFramesStateChange(
        frame_cu->lifecycle_state() ==
                resource_coordinator::mojom::LifecycleState::kFrozen
            ? -1
            : 0);
    MaybeInvalidateInterventionPolicies(frame_cu, false /* adding_frame */);
  }

  return removed;
}

void PageNodeImpl::OnNumFrozenFramesStateChange(int num_frozen_frames_delta) {
  num_frozen_frames_ += num_frozen_frames_delta;
  DCHECK_LE(num_frozen_frames_, frame_coordination_units_.size());

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
  bool is_fully_frozen = frame_coordination_units_.size() > 0 &&
                         num_frozen_frames_ == frame_coordination_units_.size();
  if (was_fully_frozen == is_fully_frozen)
    return;

  if (is_fully_frozen) {
    // Aggregate the beforeunload handler information from the entire frame
    // tree.
    bool has_nonempty_beforeunload = false;
    for (auto* frame : frame_coordination_units_) {
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
  for (size_t i = 0; i <= kMaxInterventionIndex; ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;
}

void PageNodeImpl::MaybeInvalidateInterventionPolicies(FrameNodeImpl* frame_cu,
                                                       bool adding_frame) {
  // Ensure that the frame was already added or removed as expected.
  DCHECK(adding_frame == frame_coordination_units_.count(frame_cu));

  // Determine whether or not the frames had all reported prior to this change.
  const size_t prior_frame_count =
      frame_coordination_units_.size() + (adding_frame ? -1 : 1);
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
  const bool frame_reported = frame_cu->AreAllInterventionPoliciesSet();
  if (frame_reported)
    intervention_policy_frames_reported_ += adding_frame ? 1 : -1;

  DCHECK_LE(intervention_policy_frames_reported_,
            frame_coordination_units_.size());
}

void PageNodeImpl::RecomputeInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention) {
  const size_t kIndex = ToIndex(intervention);

  // This should never be called with an empty frame tree.
  DCHECK(!frame_coordination_units_.empty());

  resource_coordinator::mojom::InterventionPolicy policy =
      resource_coordinator::mojom::InterventionPolicy::kDefault;
  for (auto* frame : frame_coordination_units_) {
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
