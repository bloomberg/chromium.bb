// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_
#define CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_

#include <unordered_map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_introspector.mojom.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace ukm {
class UkmRecorder;
}

// This class asynchronously fetches memory metrics for each process, and then
// emits UMA metrics from those metrics.
// Each instance is self-owned, and will delete itself once it has finished
// emitting metrics.
// This class is an analog to MetricsMemoryDetails, but uses the resource
// coordinator service to fetch data, rather than doing all the processing
// manually.
class ProcessMemoryMetricsEmitter
    : public base::RefCountedThreadSafe<ProcessMemoryMetricsEmitter> {
 public:
  ProcessMemoryMetricsEmitter();

  // This must be called on the main thread of the browser process.
  void FetchAndEmitProcessMemoryMetrics();

  // Public for testing.
  void MarkServiceRequestsInProgress();

 protected:
  virtual ~ProcessMemoryMetricsEmitter();

  // Virtual for testing. Callback invoked when memory_instrumentation service
  // is finished taking a memory dump.
  virtual void ReceivedMemoryDump(
      bool success,
      uint64_t dump_guid,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr ptr);

  // Virtual for testing. Callback invoked when resource_coordinator service
  // returns info for each process.
  virtual void ReceivedProcessInfos(
      std::vector<resource_coordinator::mojom::ProcessInfoPtr> process_infos);

  // Virtual for testing. Whether the GRC is enabled.
  virtual bool IsResourceCoordinatorEnabled();

  // Virtual for testing.
  virtual ukm::UkmRecorder* GetUkmRecorder();

  // Virtual for testing. Returns the number of extensions in the given process.
  // It excludes hosted apps extensions.
  virtual int GetNumberOfExtensions(base::ProcessId pid);

  // Virtual for testing. Returns the process uptime of the given process. Does
  // not return a value when the process startup time is not set.
  virtual base::Optional<base::TimeDelta> GetProcessUptime(
      const base::Time& now,
      base::ProcessId pid);

 private:
  friend class base::RefCountedThreadSafe<ProcessMemoryMetricsEmitter>;

  // This class sends two asynchronous service requests, whose results need to
  // be collated.
  void CollateResults();

  memory_instrumentation::mojom::CoordinatorPtr coordinator_;
  resource_coordinator::mojom::CoordinationUnitIntrospectorPtr introspector_;

  // The results of each request are cached. When both requests are finished,
  // the results are collated.
  bool memory_dump_in_progress_ = false;
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump_;
  bool get_process_urls_in_progress_ = false;

  // The key is ProcessInfoPtr::pid.
  std::unordered_map<int64_t, resource_coordinator::mojom::ProcessInfoPtr>
      process_infos_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitter);
};

#endif  // CHROME_BROWSER_METRICS_PROCESS_MEMORY_METRICS_EMITTER_H_
