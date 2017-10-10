// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_memory_tracing_observer.h"

#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "content/browser/tracing/background_tracing_rule.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

using base::trace_event::MemoryDumpManager;

namespace content {
namespace {
const char kEnableHeapProfilerModeName[] = "enable_heap_profiler_mode";
const char kBackgroundModeName[] = "background";

void GlobalDumpCallback(bool success, uint64_t guid) {
  // Disable heap profiling after capturing the first memory dump.
  MemoryDumpManager::GetInstance()->EnableHeapProfiling(
      base::trace_event::kHeapProfilingModeDisabled);
}
}  // namespace

// static
BackgroundMemoryTracingObserver*
BackgroundMemoryTracingObserver::GetInstance() {
  static auto* instance = new BackgroundMemoryTracingObserver();
  return instance;
}

BackgroundMemoryTracingObserver::BackgroundMemoryTracingObserver() {}
BackgroundMemoryTracingObserver::~BackgroundMemoryTracingObserver() {}

void BackgroundMemoryTracingObserver::OnScenarioActivated(
    const BackgroundTracingConfigImpl* config) {
  if (!config) {
    DCHECK(!heap_profiling_enabled_);
    return;
  }

  for (const auto& rule : config->rules()) {
    if (rule->category_preset() != BackgroundTracingConfigImpl::CategoryPreset::
                                       BENCHMARK_MEMORY_LIGHT ||
        !rule->args()) {
      continue;
    }
    std::string mode;
    if (rule->args()->GetString(kEnableHeapProfilerModeName, &mode) &&
        mode == kBackgroundModeName) {
      heap_profiling_enabled_ = true;
      // TODO(ssid): Add ability to enable profiling on all processes,
      // crbug.com/700245.
      MemoryDumpManager::GetInstance()->EnableHeapProfiling(
          base::trace_event::kHeapProfilingModeBackground);
    }
  }
}

void BackgroundMemoryTracingObserver::OnScenarioAborted() {
  if (!heap_profiling_enabled_)
    return;
  heap_profiling_enabled_ = false;
  MemoryDumpManager::GetInstance()->EnableHeapProfiling(
      base::trace_event::kHeapProfilingModeDisabled);
}

void BackgroundMemoryTracingObserver::OnTracingEnabled(
    BackgroundTracingConfigImpl::CategoryPreset preset) {
  if (preset !=
      BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_MEMORY_LIGHT)
    return;

  memory_instrumentation::MemoryInstrumentation::
      RequestGlobalDumpAndAppendToTraceCallback callback;
  if (heap_profiling_enabled_)
    callback = base::Bind(&GlobalDumpCallback);
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
          base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND, callback);
}

}  // namespace content
