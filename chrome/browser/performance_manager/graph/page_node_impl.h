// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_attached_data.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

class FrameNodeImpl;
class ProcessNodeImpl;

class PageNodeImpl : public TypedNodeBase<PageNodeImpl> {
 public:
  using LifecycleState = resource_coordinator::mojom::LifecycleState;

  static constexpr resource_coordinator::CoordinationUnitType Type() {
    return resource_coordinator::CoordinationUnitType::kPage;
  }

  explicit PageNodeImpl(Graph* graph);
  ~PageNodeImpl() override;

  void SetIsLoading(bool is_loading);
  void SetIsVisible(bool is_visible);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnMainFrameNavigationCommitted(base::TimeTicks navigation_committed_time,
                                      int64_t navigation_id,
                                      const std::string& url);

  // There is no direct relationship between processes and pages. However,
  // frames are accessible by both processes and frames, so we find all of the
  // processes that are reachable from the pages's accessible frames.
  std::set<ProcessNodeImpl*> GetAssociatedProcessCoordinationUnits() const;

  // Returns the average CPU usage that can be attributed to this page over the
  // last measurement period. CPU usage is expressed as the average percentage
  // of cores occupied over the last measurement interval. One core fully
  // occupied would be 100, while two cores at 5% each would be 10.
  double GetCPUUsage() const;

  // Returns 0 if no navigation has happened, otherwise returns the time since
  // the last navigation commit.
  base::TimeDelta TimeSinceLastNavigation() const;

  // Returns the time since the last visibility change, it should always have a
  // value since we set the visibility property when we create a
  // PageCoordinationUnit.
  base::TimeDelta TimeSinceLastVisibilityChange() const;

  std::vector<FrameNodeImpl*> GetFrameNodes() const;

  // Returns the main frame CU or nullptr if this page has no main frame.
  FrameNodeImpl* GetMainFrameNode() const;

  // Accessors.
  bool is_visible() const { return is_visible_.value(); }
  bool is_loading() const { return is_loading_.value(); }
  ukm::SourceId ukm_source_id() const { return ukm_source_id_.value(); }
  LifecycleState lifecycle_state() const { return lifecycle_state_.value(); }
  const std::set<FrameNodeImpl*>& main_frame_nodes() const {
    return main_frame_nodes_;
  }
  base::TimeTicks usage_estimate_time() const { return usage_estimate_time_; }
  void set_usage_estimate_time(base::TimeTicks usage_estimate_time) {
    usage_estimate_time_ = usage_estimate_time;
  }
  base::TimeDelta cumulative_cpu_usage_estimate() const {
    return cumulative_cpu_usage_estimate_;
  }
  void set_cumulative_cpu_usage_estimate(
      base::TimeDelta cumulative_cpu_usage_estimate) {
    cumulative_cpu_usage_estimate_ = cumulative_cpu_usage_estimate;
  }
  uint64_t private_footprint_kb_estimate() const {
    return private_footprint_kb_estimate_;
  }
  void set_private_footprint_kb_estimate(
      uint64_t private_footprint_kb_estimate) {
    private_footprint_kb_estimate_ = private_footprint_kb_estimate;
  }
  void set_has_nonempty_beforeunload(bool has_nonempty_beforeunload) {
    has_nonempty_beforeunload_ = has_nonempty_beforeunload;
  }
  bool page_almost_idle() const { return page_almost_idle_.value(); }

  const std::string& main_frame_url() const { return main_frame_url_; }
  int64_t navigation_id() const { return navigation_id_; }

  // Invoked when the state of a frame in this page changes.
  void OnFrameLifecycleStateChanged(FrameNodeImpl* frame_node,
                                    LifecycleState old_state);

  void OnFrameInterventionPolicyChanged(
      FrameNodeImpl* frame,
      resource_coordinator::mojom::PolicyControlledIntervention intervention,
      resource_coordinator::mojom::InterventionPolicy old_policy,
      resource_coordinator::mojom::InterventionPolicy new_policy);

  // Gets the current policy for the specified |intervention|, recomputing it
  // from individual frame policies if necessary. Returns kUnknown until there
  // are 1 or more frames, and they have all computed their local policy
  // settings.
  resource_coordinator::mojom::InterventionPolicy GetInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention);

  // Similar to GetInterventionPolicy, but doesn't trigger recomputes.
  resource_coordinator::mojom::InterventionPolicy
  GetRawInterventionPolicyForTesting(
      resource_coordinator::mojom::PolicyControlledIntervention intervention)
      const {
    return intervention_policy_[static_cast<size_t>(intervention)];
  }

  size_t GetInterventionPolicyFramesReportedForTesting() const {
    return intervention_policy_frames_reported_;
  }

  void SetPageAlmostIdleForTesting(bool page_almost_idle) {
    SetPageAlmostIdle(page_almost_idle);
  }

 private:
  friend class FrameNodeImpl;
  friend class PageAlmostIdleAccess;

  void AddFrame(FrameNodeImpl* frame_node);
  void RemoveFrame(FrameNodeImpl* frame_node);
  void JoinGraph() override;
  void LeaveGraph() override;

  // Returns true iff |frame_node| is in the current frame hierarchy.
  bool HasFrame(FrameNodeImpl* frame_node);

  void SetPageAlmostIdle(bool page_almost_idle);

  // CoordinationUnitInterface implementation.
  void OnEventReceived(resource_coordinator::mojom::Event event) override;

  // This is called whenever |num_frozen_frames_| changes, or whenever
  // a frame is added to or removed from this page. It is used to synthesize the
  // value of |has_nonempty_beforeunload| and to update the LifecycleState of
  // the page. Calling this with |num_frozen_frames_delta == 0| implies that the
  // number of frames itself has changed.
  void OnNumFrozenFramesStateChange(int num_frozen_frames_delta);

  // Invalidates all currently aggregated intervention policies.
  void InvalidateAllInterventionPolicies();

  // Invoked when adding or removing a frame. This will update
  // |intervention_policy_frames_reported_| if necessary and potentially
  // invalidate the aggregated intervention policies. This should be called
  // after the frame has already been added or removed from
  // |frame_nodes_|.
  void MaybeInvalidateInterventionPolicies(FrameNodeImpl* frame_node,
                                           bool adding_frame);

  // Recomputes intervention policy aggregation. This is invoked on demand when
  // a policy is queried.
  void RecomputeInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention);

  // Invokes |map_function| for all frame nodes in this pages frame tree.
  template <typename MapFunction>
  void ForAllFrameNodes(MapFunction map_function) const;

  // The main frame nodes of this page. There can be more than one main frame
  // in a page, among other reasons because during main frame navigation, the
  // pending navigation will coexist with the existing main frame until it's
  // committed.
  std::set<FrameNodeImpl*> main_frame_nodes_;
  // The total count of frames that tally up to this page.
  size_t frame_node_count_ = 0;

  base::TimeTicks visibility_change_time_;
  // Main frame navigation committed time.
  base::TimeTicks navigation_committed_time_;

  // The time the most recent resource usage estimate applies to.
  base::TimeTicks usage_estimate_time_;

  // The most current CPU usage estimate. Note that this estimate is most
  // generously described as "piecewise linear", as it attributes the CPU
  // cost incurred since the last measurement was made equally to pages
  // hosted by a process. If, e.g. a frame has come into existence and vanished
  // from a given process between measurements, the entire cost to that frame
  // will be mis-attributed to other frames hosted in that process.
  base::TimeDelta cumulative_cpu_usage_estimate_;
  // The most current memory footprint estimate.
  uint64_t private_footprint_kb_estimate_ = 0;

  // Counts the number of frames in a page that are frozen.
  size_t num_frozen_frames_ = 0;

  // Indicates whether or not this page has a non-empty beforeunload handler.
  // This is an aggregation of the same value on each frame in the page's frame
  // tree. The aggregation is made at the moment all frames associated with a
  // page have transition to frozen.
  bool has_nonempty_beforeunload_ = false;

  // The URL the main frame last committed a navigation to and the unique ID of
  // the associated navigation handle.
  std::string main_frame_url_;
  int64_t navigation_id_ = 0;

  // The aggregate intervention policy states for this page. These are
  // aggregated from the corresponding per-frame values. If an individual value
  // is kUnknown then a frame in the frame tree has changed values and
  // a new aggregation is required.
  resource_coordinator::mojom::InterventionPolicy
      intervention_policy_[static_cast<size_t>(
                               resource_coordinator::mojom::
                                   PolicyControlledIntervention::kMaxValue) +
                           1];

  // The number of child frames that have checked in with initial intervention
  // policy values. If this doesn't match the number of known child frames, then
  // aggregation isn't possible. Child frames check in with all properties once
  // immediately after document parsing, and the *last* value being set
  // is used as a signal that the frame has reported.
  size_t intervention_policy_frames_reported_ = 0;

  // Page almost idle state. This is the output that is driven by the
  // PageAlmostIdleDecorator.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &GraphObserver::OnPageAlmostIdleChanged>
          page_almost_idle_{false};
  // Whether or not the page is visible. Driven by browser instrumentation.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &GraphObserver::OnIsVisibleChanged>
      is_visible_{false};
  // The loading state. This is driven by instrumentation in the browser
  // process.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &GraphObserver::OnIsLoadingChanged>
      is_loading_{false};
  // The UKM source ID associated with the URL of the main frame of this page.
  ObservedProperty::NotifiesOnlyOnChanges<ukm::SourceId,
                                          &GraphObserver::OnUkmSourceIdChanged>
      ukm_source_id_{ukm::kInvalidSourceId};
  // The lifecycle state of this page. This is aggregated from the lifecycle
  // state of each frame in the frame tree.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &GraphObserver::OnLifecycleStateChanged>
      lifecycle_state_{LifecycleState::kRunning};

  // Storage for PageAlmostIdle user data.
  std::unique_ptr<NodeAttachedData> page_almost_idle_data_;

  DISALLOW_COPY_AND_ASSIGN(PageNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
