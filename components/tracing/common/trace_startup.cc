// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/trace_startup.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/common/trace_config_file.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"

namespace tracing {

void EnableStartupTracingIfNeeded(bool can_access_file_system) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Enables heap profiling if "--enable-heap-profiling" flag is passed.
  base::trace_event::MemoryDumpManager::GetInstance()
      ->EnableHeapProfilingIfNeeded();

  if (command_line.HasSwitch(switches::kTraceStartup)) {
    base::trace_event::TraceConfig trace_config(
        command_line.GetSwitchValueASCII(switches::kTraceStartup),
        base::trace_event::RECORD_UNTIL_FULL);
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
  } else if (command_line.HasSwitch(switches::kTraceToConsole)) {
    base::trace_event::TraceConfig trace_config =
        tracing::GetConfigForTraceToConsole();
    LOG(ERROR) << "Start " << switches::kTraceToConsole
               << " with CategoryFilter '"
               << trace_config.ToCategoryFilterString() << "'.";
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config, base::trace_event::TraceLog::RECORDING_MODE);
  } else if (can_access_file_system &&
             tracing::TraceConfigFile::GetInstance()->IsEnabled()) {
    // This checks kTraceConfigFile switch.
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        tracing::TraceConfigFile::GetInstance()->GetTraceConfig(),
        base::trace_event::TraceLog::RECORDING_MODE);
  }
}

}  // namespace tracing
