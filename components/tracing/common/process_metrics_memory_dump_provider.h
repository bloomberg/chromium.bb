// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_
#define COMPONENTS_TRACING_COMMON_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_

#include "base/process/process_handle.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/tracing/tracing_export.h"

// TODO(hjd): The actual impl lives in service/resource_coordinator. Update the
// callsites and remove this file.
namespace tracing {

// Dump provider which collects process-wide memory stats.
class TRACING_EXPORT ProcessMetricsMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Pass base::kNullProcessId to register for current process.
  static void RegisterForProcess(base::ProcessId process);
  static void UnregisterForProcess(base::ProcessId process);


 private:
  ProcessMetricsMemoryDumpProvider();
  ~ProcessMetricsMemoryDumpProvider() override;
  DISALLOW_COPY_AND_ASSIGN(ProcessMetricsMemoryDumpProvider);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_
