// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_

#include <set>

#include "base/macros.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/node_base.h"

namespace performance_manager {

class FrameNodeImpl;

// A process coordination unit follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process CU goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or falied to start, have exit status.
// 4. Back to 2.
class ProcessNodeImpl
    : public CoordinationUnitInterface<
          ProcessNodeImpl,
          resource_coordinator::mojom::ProcessCoordinationUnit,
          resource_coordinator::mojom::ProcessCoordinationUnitRequest> {
 public:
  static constexpr resource_coordinator::CoordinationUnitType Type() {
    return resource_coordinator::CoordinationUnitType::kProcess;
  }

  ProcessNodeImpl(const resource_coordinator::CoordinationUnitID& id,
                  Graph* graph);
  ~ProcessNodeImpl() override;

  // resource_coordinator::mojom::ProcessCoordinationUnit implementation.
  void SetCPUUsage(double cpu_usage) override;
  void SetExpectedTaskQueueingDuration(base::TimeDelta duration) override;
  void SetLaunchTime(base::Time launch_time) override;
  void SetMainThreadTaskLoadIsLow(bool main_thread_task_load_is_low) override;
  void SetPID(base::ProcessId pid) override;
  void SetProcessExitStatus(int32_t exit_status) override;
  void OnRendererIsBloated() override;

  // Private implementation properties.
  void set_private_footprint_kb(uint64_t private_footprint_kb) {
    private_footprint_kb_ = private_footprint_kb;
  }
  uint64_t private_footprint_kb() const { return private_footprint_kb_; }
  void set_cumulative_cpu_usage(base::TimeDelta cumulative_cpu_usage) {
    cumulative_cpu_usage_ = cumulative_cpu_usage;
  }
  base::TimeDelta cumulative_cpu_usage() const { return cumulative_cpu_usage_; }

  const std::set<FrameNodeImpl*>& GetFrameNodes() const;
  std::set<PageNodeImpl*> GetAssociatedPageCoordinationUnits() const;

  base::ProcessId process_id() const { return process_id_; }
  base::Time launch_time() const { return launch_time_; }
  base::Optional<int32_t> exit_status() const { return exit_status_; }

  // Add |frame_cu| to this process.
  void AddFrame(FrameNodeImpl* frame_cu);
  // Removes |frame_cu| from the set of frames hosted by this process. Invoked
  // from the destructor of FrameNodeImpl.
  void RemoveFrame(FrameNodeImpl* frame_cu);

  // Invoked when the state of a frame hosted by this process changes.
  void OnFrameLifecycleStateChanged(
      FrameNodeImpl* frame_cu,
      resource_coordinator::mojom::LifecycleState old_state);

 private:
  // CoordinationUnitInterface implementation.
  void OnEventReceived(resource_coordinator::mojom::Event event) override;
  void OnPropertyChanged(
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;

  void DecrementNumFrozenFrames();
  void IncrementNumFrozenFrames();

  base::TimeDelta cumulative_cpu_usage_;
  uint64_t private_footprint_kb_ = 0u;

  base::ProcessId process_id_ = base::kNullProcessId;
  base::Time launch_time_;
  base::Optional<int32_t> exit_status_;

  std::set<FrameNodeImpl*> frame_coordination_units_;

  // The number of frames hosted by this process that are frozen.
  int num_frozen_frames_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ProcessNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_PROCESS_NODE_IMPL_H_
