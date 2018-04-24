// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_

#include <map>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/timer/timer.h"

namespace resource_coordinator {

class SystemResourceCoordinator;

struct RenderProcessInfo {
  RenderProcessInfo();
  ~RenderProcessInfo();
  base::Process process;
  double cpu_usage = -1.0;
  // This structure bounces from the UI thread to blocking threads and back.
  // It's therefore not safe to store RenderProcessHost pointers, so the ID is
  // used instead.
  int render_process_host_id = 0;
  size_t last_gather_cycle_active;
  std::unique_ptr<base::ProcessMetrics> metrics;
};

using RenderProcessInfoMap = std::map<int, RenderProcessInfo>;

// |ResourceCoordinatorRenderProcessProbe| collects measurements about render
// processes and propagates them to the |resource_coordinator| service.
// Currently this is only supported for Chrome Metrics experiments.
// The measurements are initiated and results are dispatched from the UI
// thread, while acquiring the measurements is done on the IO thread.
class ResourceCoordinatorRenderProcessProbe {
 public:
  // Returns the current |ResourceCoordinatorRenderProcessProbe| instance
  // if one exists; otherwise it constructs a new instance.
  static ResourceCoordinatorRenderProcessProbe* GetInstance();

  static bool IsEnabled();

  // Starts the automatic, timed process metrics collection cycle.
  // Can only be invoked from the UI thread.
  void StartGatherCycle();

  // Starts a single immediate collection cycle, if a cycle is not already
  // in progress. If the timed gather cycle is running, this will preempt the
  // next cycle and reset the metronome.
  void StartSingleGather();

 protected:
  // Internal state protected for testing.
  friend struct base::LazyInstanceTraitsBase<
      ResourceCoordinatorRenderProcessProbe>;

  ResourceCoordinatorRenderProcessProbe();
  virtual ~ResourceCoordinatorRenderProcessProbe();

  // (1) Identify all of the render processes that are active to measure.
  // Child render processes can only be discovered in the browser's UI thread.
  void RegisterAliveRenderProcessesOnUIThread();
  // (2) Collect render process metrics.
  void CollectRenderProcessMetricsOnIOThread();
  // (3) Send the collected render process metrics to the appropriate
  // coordination units in the |resource_coordinator| service. Then
  // initiates the next render process metrics collection cycle, which
  // consists of a delayed call to perform (1) via a timer.
  void HandleRenderProcessMetricsOnUIThread();

  // Allows FieldTrial parameters to override defaults.
  void UpdateWithFieldTrialParams();

  SystemResourceCoordinator* EnsureSystemResourceCoordinator();

  // Dispatch the collected metrics. Returns |true| if another metrics
  // collection gather cycle should be initiated. Virtual for testing.
  // Default implementation sends collected metrics back to the resource
  // coordinator service and initiates another render process metrics gather
  // cycle.
  virtual bool DispatchMetrics();

  // A map of currently running render process host IDs to Process.
  RenderProcessInfoMap render_process_info_map_;

  // Time duration between measurements.
  base::TimeDelta interval_;

  // Timer to signal the |ResourceCoordinatorRenderProcessProbe| instance
  // to conduct its measurements as a regular interval;
  base::OneShotTimer timer_;

  // Number of measurements collected so far.
  size_t current_gather_cycle_ = 0u;

  // True if StartGatherCycle has been called.
  bool is_gather_cycle_started_ = false;

  // True while a gathering cycle is underways on a background thread.
  bool is_gathering_ = false;

  // Used to signal the end of a CPU measurement cycle to the RC.
  std::unique_ptr<SystemResourceCoordinator> system_resource_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorRenderProcessProbe);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_
