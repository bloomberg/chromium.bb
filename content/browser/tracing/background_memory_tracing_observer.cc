// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/background_memory_tracing_observer.h"

#include "base/trace_event/memory_dump_manager.h"

namespace content {

// static
BackgroundMemoryTracingObserver*
BackgroundMemoryTracingObserver::GetInstance() {
  static auto* instance = new BackgroundMemoryTracingObserver;
  return instance;
}

BackgroundMemoryTracingObserver::BackgroundMemoryTracingObserver() {}
BackgroundMemoryTracingObserver::~BackgroundMemoryTracingObserver() {}

void BackgroundMemoryTracingObserver::OnScenarioActivated(
    const BackgroundTracingConfigImpl* config) {}

void BackgroundMemoryTracingObserver::OnTracingEnabled(
    BackgroundTracingConfigImpl::CategoryPreset preset) {
  if (preset ==
      BackgroundTracingConfigImpl::CategoryPreset::BENCHMARK_MEMORY_LIGHT) {
    auto* dump_manager = base::trace_event::MemoryDumpManager::GetInstance();
    dump_manager->RequestGlobalDump(
        base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
        base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND);
  }
}

}  // namespace content
