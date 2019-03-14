// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/node_base.h"

namespace performance_manager {

class PageNodeImpl;
class ProcessNodeImpl;

// Frame Coordination Units form a tree structure, each FrameNode at
// most has one parent that is a FrameNode.
// A Frame Coordination Unit will have parents only if navigation committed.
class FrameNodeImpl
    : public CoordinationUnitInterface<
          FrameNodeImpl,
          resource_coordinator::mojom::FrameCoordinationUnit,
          resource_coordinator::mojom::FrameCoordinationUnitRequest> {
 public:
  static constexpr resource_coordinator::CoordinationUnitType Type() {
    return resource_coordinator::CoordinationUnitType::kFrame;
  }

  FrameNodeImpl(const resource_coordinator::CoordinationUnitID& id,
                Graph* graph);
  ~FrameNodeImpl() override;

  // FrameNode implementation.
  void SetProcess(
      const resource_coordinator::CoordinationUnitID& cu_id) override;
  void AddChildFrame(
      const resource_coordinator::CoordinationUnitID& cu_id) override;
  void RemoveChildFrame(
      const resource_coordinator::CoordinationUnitID& cu_id) override;
  void SetNetworkAlmostIdle(bool idle) override;
  void SetLifecycleState(
      resource_coordinator::mojom::LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention,
      resource_coordinator::mojom::InterventionPolicy policy) override;
  void OnNonPersistentNotificationCreated() override;

  FrameNodeImpl* GetParentFrameNode() const;
  PageNodeImpl* GetPageNode() const;
  ProcessNodeImpl* GetProcessNode() const;
  bool IsMainFrame() const;

  resource_coordinator::mojom::LifecycleState lifecycle_state() const {
    return lifecycle_state_;
  }
  bool has_nonempty_beforeunload() const { return has_nonempty_beforeunload_; }
  bool network_almost_idle() const { return network_almost_idle_; }

  // Returns true if all intervention policies have been set for this frame.
  bool AreAllInterventionPoliciesSet() const;

  const std::set<FrameNodeImpl*>& child_frame_coordination_units_for_testing()
      const {
    return child_frame_coordination_units_;
  }

  // Sets the same policy for all intervention types in this frame. Causes
  // Page::OnFrameInterventionPolicyChanged to be invoked.
  void SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy policy);

 private:
  friend class PageNodeImpl;
  friend class ProcessNodeImpl;

  // CoordinationUnitInterface implementation.
  void OnEventReceived(resource_coordinator::mojom::Event event) override;
  void OnPropertyChanged(
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;

  bool HasFrameNodeInAncestors(FrameNodeImpl* frame_cu) const;
  bool HasFrameNodeInDescendants(FrameNodeImpl* frame_cu) const;

  // The following methods will be called by other FrameNodeImpl,
  // PageNodeImpl and ProcessNodeImpl respectively to
  // manipulate their relationship.
  void AddParentFrame(FrameNodeImpl* parent_frame_cu);
  bool AddChildFrame(FrameNodeImpl* child_frame_cu);
  void RemoveParentFrame(FrameNodeImpl* parent_frame_cu);
  bool RemoveChildFrame(FrameNodeImpl* child_frame_cu);
  void AddPageNode(PageNodeImpl* page_cu);
  void AddProcessNode(ProcessNodeImpl* process_cu);
  void RemovePageNode(PageNodeImpl* page_cu);
  void RemoveProcessNode(ProcessNodeImpl* process_cu);

  FrameNodeImpl* parent_frame_coordination_unit_;
  PageNodeImpl* page_coordination_unit_;
  ProcessNodeImpl* process_coordination_unit_;
  std::set<FrameNodeImpl*> child_frame_coordination_units_;

  resource_coordinator::mojom::LifecycleState lifecycle_state_ =
      resource_coordinator::mojom::LifecycleState::kRunning;
  bool has_nonempty_beforeunload_ = false;
  // Network is considered almost idle when there are no more than 2 network
  // connections.
  bool network_almost_idle_ = false;

  // Intervention policy for this frame. These are communicated from the
  // renderer process and are controlled by origin trials.
  resource_coordinator::mojom::InterventionPolicy
      intervention_policy_[static_cast<size_t>(
                               resource_coordinator::mojom::
                                   PolicyControlledIntervention::kMaxValue) +
                           1];

  DISALLOW_COPY_AND_ASSIGN(FrameNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
