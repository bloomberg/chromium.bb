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
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/timer/timer.h"

namespace content {
class RenderProcessHost;
}

namespace resource_coordinator {

struct RenderProcessInfo {
  RenderProcessInfo();
  ~RenderProcessInfo();
  double cpu_usage = -1.0;
  content::RenderProcessHost* host = nullptr;
  size_t last_gather_cycle_active;
  std::unique_ptr<base::ProcessMetrics> metrics;
};

using RenderProcessInfoMap = std::map<base::ProcessHandle, RenderProcessInfo>;

// A delegate class for handling metrics collected after each
// |ResourceCoordinatorRenderProcessProbe| gather cycle.
class RenderProcessMetricsHandler {
 public:
  RenderProcessMetricsHandler();
  virtual ~RenderProcessMetricsHandler();
  // Handle collected metrics. Returns |true| if the
  // |ResourceCoordinatorRenderProcessProbe| should initiate another
  // metrics collection gather cycle.
  virtual bool HandleMetrics(
      const RenderProcessInfoMap& render_process_info_map) = 0;
};

// |ResourceCoordinatorRenderProcessProbe| collects measurements about render
// processes and propagates them to the |resource_coordinator| service.
// Currently this is only supported for Chrome Metrics experiments.
class ResourceCoordinatorRenderProcessProbe {
 public:
  // Returns the current |ResourceCoordinatorRenderProcessProbe| instance
  // if one exists; otherwise it constructs a new instance.
  static ResourceCoordinatorRenderProcessProbe* GetInstance();

  static bool IsEnabled();

  // Render process metrics collection cycle:
  // (0) Initialize and begin render process metrics collection.
  void StartGatherCycle();
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

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourceCoordinatorRenderProcessProbeBrowserTest,
                           TrackAndMeasureActiveRenderProcesses);
  friend struct base::LazyInstanceTraitsBase<
      ResourceCoordinatorRenderProcessProbe>;

  ResourceCoordinatorRenderProcessProbe();
  ~ResourceCoordinatorRenderProcessProbe();

  // Delegate for handling metrics collected after each gather cycle.
  std::unique_ptr<RenderProcessMetricsHandler> metrics_handler_;

  // A map of currently running ProcessHandles to Process.
  RenderProcessInfoMap render_process_info_map_;

  // Time duration between measurements in milliseconds.
  base::TimeDelta interval_ms_;

  // Timer to signal the |ResourceCoordinatorRenderProcessProbe| instance
  // to conduct its measurements as a regular interval;
  base::OneShotTimer timer_;

  // Number of measurements collected so far.
  size_t current_gather_cycle_ = 0u;

  // Allows FieldTrial parameters to override defaults.
  void UpdateWithFieldTrialParams();

  // Settings/getters for testing.
  void set_render_process_metrics_handler_for_testing(
      std::unique_ptr<RenderProcessMetricsHandler> metrics_handler) {
    metrics_handler_ = std::move(metrics_handler);
  }
  const RenderProcessInfoMap& render_process_info_map_for_testing() const {
    return render_process_info_map_;
  }
  size_t current_gather_cycle_for_testing() const {
    return current_gather_cycle_;
  }

  // Returns |true| if all of the elements in |render_process_info_map_|
  // are up-to-date with the |current_gather_cycle_|.
  bool AllRenderProcessMeasurementsAreCurrentForTesting() const;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorRenderProcessProbe);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_
