// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/process_node_impl.h"

#include "base/logging.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

ProcessNodeImpl::ProcessNodeImpl(Graph* graph)
    : CoordinationUnitInterface(graph) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProcessNodeImpl::~ProcessNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProcessNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  const bool inserted = frame_nodes_.insert(frame_node).second;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(inserted);
  if (frame_node->lifecycle_state() ==
      resource_coordinator::mojom::LifecycleState::kFrozen)
    IncrementNumFrozenFrames();
}

void ProcessNodeImpl::SetCPUUsage(double cpu_usage) {
  cpu_usage_ = cpu_usage;
}

void ProcessNodeImpl::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  expected_task_queueing_duration_.SetAndNotify(this, duration);
}

void ProcessNodeImpl::SetLaunchTime(base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(launch_time_.is_null());
  launch_time_ = launch_time;
}

void ProcessNodeImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  main_thread_task_load_is_low_.SetAndMaybeNotify(this,
                                                  main_thread_task_load_is_low);
}

void ProcessNodeImpl::SetPID(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Either this is the initial process associated with this process CU,
  // or it's a subsequent process. In the latter case, there must have been
  // an exit status associated with the previous process.
  DCHECK(process_id_ == base::kNullProcessId || exit_status_.has_value());

  graph()->BeforeProcessPidChange(this, pid);

  process_id_ = pid;

  // Clear launch time and exit status for the previous process (if any).
  launch_time_ = base::Time();
  exit_status_.reset();

  // Also clear the measurement data (if any), as it references the previous
  // process.
  private_footprint_kb_ = 0;
  cumulative_cpu_usage_ = base::TimeDelta();
}

void ProcessNodeImpl::SetProcessExitStatus(int32_t exit_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  exit_status_ = exit_status;
}

void ProcessNodeImpl::OnRendererIsBloated() {
  SendEvent(resource_coordinator::mojom::Event::kRendererIsBloated);
}

const std::set<FrameNodeImpl*>& ProcessNodeImpl::GetFrameNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_nodes_;
}

// There is currently not a direct relationship between processes and
// pages. However, frames are children of both processes and frames, so we
// find all of the pages that are reachable from the process's child
// frames.
std::set<PageNodeImpl*> ProcessNodeImpl::GetAssociatedPageCoordinationUnits()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<PageNodeImpl*> page_nodes;
  for (auto* frame_node : frame_nodes_) {
    if (auto* page_node = frame_node->GetPageNode())
      page_nodes.insert(page_node);
  }
  return page_nodes;
}

void ProcessNodeImpl::OnFrameLifecycleStateChanged(
    FrameNodeImpl* frame_node,
    resource_coordinator::mojom::LifecycleState old_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ContainsKey(frame_nodes_, frame_node));
  DCHECK_NE(old_state, frame_node->lifecycle_state());

  if (old_state == resource_coordinator::mojom::LifecycleState::kFrozen)
    DecrementNumFrozenFrames();
  else if (frame_node->lifecycle_state() ==
           resource_coordinator::mojom::LifecycleState::kFrozen)
    IncrementNumFrozenFrames();
}

void ProcessNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  for (auto* child_frame : frame_nodes_)
    child_frame->RemoveProcessNode(this);
}

void ProcessNodeImpl::OnEventReceived(
    resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnProcessEventReceived(this, event);
}

void ProcessNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ContainsKey(frame_nodes_, frame_node));
  frame_nodes_.erase(frame_node);

  if (frame_node->lifecycle_state() ==
      resource_coordinator::mojom::LifecycleState::kFrozen)
    DecrementNumFrozenFrames();
}

void ProcessNodeImpl::DecrementNumFrozenFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --num_frozen_frames_;
  DCHECK_GE(num_frozen_frames_, 0);
}

void ProcessNodeImpl::IncrementNumFrozenFrames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ++num_frozen_frames_;
  DCHECK_LE(num_frozen_frames_, static_cast<int>(frame_nodes_.size()));

  if (num_frozen_frames_ == static_cast<int>(frame_nodes_.size())) {
    for (auto& observer : observers())
      observer.OnAllFramesInProcessFrozen(this);
  }
}

}  // namespace performance_manager
