// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/process_node_impl.h"

#include "base/logging.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"

namespace performance_manager {

ProcessNodeImpl::ProcessNodeImpl(
    const resource_coordinator::CoordinationUnitID& id,
    Graph* graph)
    : CoordinationUnitInterface(id, graph) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProcessNodeImpl::~ProcessNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make as if we're transitioning to the null PID before we die to clear this
  // instance from the PID map.
  if (process_id_ != base::kNullProcessId)
    graph()->BeforeProcessPidChange(this, base::kNullProcessId);

  for (auto* child_frame : frame_coordination_units_)
    child_frame->RemoveProcessNode(this);
}

void ProcessNodeImpl::AddFrame(FrameNodeImpl* frame_cu) {
  const bool inserted = frame_coordination_units_.insert(frame_cu).second;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(inserted);
  if (frame_cu->lifecycle_state() ==
      resource_coordinator::mojom::LifecycleState::kFrozen)
    IncrementNumFrozenFrames();
}

void ProcessNodeImpl::SetCPUUsage(double cpu_usage) {
  SetProperty(resource_coordinator::mojom::PropertyType::kCPUUsage,
              cpu_usage * 1000);
}

void ProcessNodeImpl::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  SetProperty(
      resource_coordinator::mojom::PropertyType::kExpectedTaskQueueingDuration,
      duration.InMilliseconds());
}

void ProcessNodeImpl::SetLaunchTime(base::Time launch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(launch_time_.is_null());
  launch_time_ = launch_time;
}

void ProcessNodeImpl::SetMainThreadTaskLoadIsLow(
    bool main_thread_task_load_is_low) {
  SetProperty(
      resource_coordinator::mojom::PropertyType::kMainThreadTaskLoadIsLow,
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

  SetProperty(resource_coordinator::mojom::PropertyType::kPID, pid);
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
  return frame_coordination_units_;
}

// There is currently not a direct relationship between processes and
// pages. However, frames are children of both processes and frames, so we
// find all of the pages that are reachable from the process's child
// frames.
std::set<PageNodeImpl*> ProcessNodeImpl::GetAssociatedPageCoordinationUnits()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<PageNodeImpl*> page_cus;
  for (auto* frame_cu : frame_coordination_units_) {
    if (auto* page_cu = frame_cu->GetPageNode())
      page_cus.insert(page_cu);
  }
  return page_cus;
}

void ProcessNodeImpl::OnFrameLifecycleStateChanged(
    FrameNodeImpl* frame_cu,
    resource_coordinator::mojom::LifecycleState old_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ContainsKey(frame_coordination_units_, frame_cu));
  DCHECK_NE(old_state, frame_cu->lifecycle_state());

  if (old_state == resource_coordinator::mojom::LifecycleState::kFrozen)
    DecrementNumFrozenFrames();
  else if (frame_cu->lifecycle_state() ==
           resource_coordinator::mojom::LifecycleState::kFrozen)
    IncrementNumFrozenFrames();
}

void ProcessNodeImpl::OnEventReceived(
    resource_coordinator::mojom::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnProcessEventReceived(this, event);
}

void ProcessNodeImpl::OnPropertyChanged(
    const resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnProcessPropertyChanged(this, property_type, value);
}

void ProcessNodeImpl::RemoveFrame(FrameNodeImpl* frame_cu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ContainsKey(frame_coordination_units_, frame_cu));
  frame_coordination_units_.erase(frame_cu);

  if (frame_cu->lifecycle_state() ==
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
  DCHECK_LE(num_frozen_frames_,
            static_cast<int>(frame_coordination_units_.size()));

  if (num_frozen_frames_ ==
      static_cast<int>(frame_coordination_units_.size())) {
    for (auto& observer : observers())
      observer.OnAllFramesInProcessFrozen(this);
  }
}

}  // namespace performance_manager
