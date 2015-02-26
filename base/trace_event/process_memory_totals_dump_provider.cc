// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_totals_dump_provider.h"

#include "base/process/process_metrics.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_totals.h"

namespace base {
namespace trace_event {

// static
uint64 ProcessMemoryTotalsDumpProvider::rss_bytes_for_testing = 0;

namespace {

const char kDumperFriendlyName[] = "ProcessMemoryTotals";

ProcessMetrics* CreateProcessMetricsForCurrentProcess() {
#if !defined(OS_MACOSX) || defined(OS_IOS)
  return ProcessMetrics::CreateProcessMetrics(GetCurrentProcessHandle());
#else
  return ProcessMetrics::CreateProcessMetrics(GetCurrentProcessHandle(), NULL);
#endif
}
}  // namespace

// static
ProcessMemoryTotalsDumpProvider*
ProcessMemoryTotalsDumpProvider::GetInstance() {
  return Singleton<
      ProcessMemoryTotalsDumpProvider,
      LeakySingletonTraits<ProcessMemoryTotalsDumpProvider>>::get();
}

ProcessMemoryTotalsDumpProvider::ProcessMemoryTotalsDumpProvider()
    : process_metrics_(CreateProcessMetricsForCurrentProcess()) {
}

ProcessMemoryTotalsDumpProvider::~ProcessMemoryTotalsDumpProvider() {
}

// Called at trace dump point time. Creates a snapshot the memory counters for
// the current process.
bool ProcessMemoryTotalsDumpProvider::DumpInto(ProcessMemoryDump* pmd) {
  const uint64 rss_bytes = rss_bytes_for_testing
                               ? rss_bytes_for_testing
                               : process_metrics_->GetWorkingSetSize();

  if (rss_bytes > 0) {
    pmd->process_totals()->set_resident_set_bytes(rss_bytes);
    pmd->set_has_process_totals();
    return true;
  }

  return false;
}

const char* ProcessMemoryTotalsDumpProvider::GetFriendlyName() const {
  return kDumperFriendlyName;
}

}  // namespace trace_event
}  // namespace base
