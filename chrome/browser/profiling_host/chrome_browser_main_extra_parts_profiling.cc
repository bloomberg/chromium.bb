// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/chrome_browser_main_extra_parts_profiling.h"

#include "base/command_line.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/common/chrome_switches.h"

ChromeBrowserMainExtraPartsProfiling::ChromeBrowserMainExtraPartsProfiling() =
    default;
ChromeBrowserMainExtraPartsProfiling::~ChromeBrowserMainExtraPartsProfiling() =
    default;

void ChromeBrowserMainExtraPartsProfiling::ServiceManagerConnectionStarted(
    content::ServiceManagerConnection* connection) {
#if defined(ADDRESS_SANITIZER)
  // Memory sanitizers are using large memory shadow to keep track of memory
  // state. Using memlog and memory sanitizers at the same time is slowing down
  // user experience, causing the browser to be barely responsive. In theory,
  // memlog and memory sanitizers are compatible and can run at the same time.
  (void)connection;  // Unused variable.
#else
  profiling::ProfilingProcessHost::Mode mode =
      profiling::ProfilingProcessHost::GetModeForStartup();
  if (mode != profiling::ProfilingProcessHost::Mode::kNone) {
    profiling::ProfilingProcessHost::Start(
        connection, mode,
        profiling::ProfilingProcessHost::GetStackModeForStartup(),
        profiling::ProfilingProcessHost::GetShouldSampleForStartup(),
        profiling::ProfilingProcessHost::GetSamplingRateForStartup());
  }
#endif
}
