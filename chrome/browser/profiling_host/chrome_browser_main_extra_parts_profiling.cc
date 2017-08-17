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
  const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();
  if (!cmdline.HasSwitch(switches::kMemlog))
    return;

  std::string profiling_mode =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMemlog);
  if (profiling_mode == switches::kMemlogModeAll) {
    profiling::ProfilingProcessHost::EnsureStarted(
        connection, profiling::ProfilingProcessHost::Mode::kAll);
    return;
  }
  if (profiling_mode == switches::kMemlogModeBrowser) {
    profiling::ProfilingProcessHost::EnsureStarted(
        connection, profiling::ProfilingProcessHost::Mode::kBrowser);
    return;
  }
  LOG(ERROR) << "Unsupported value: \"" << profiling_mode << "\" passed to --"
             << switches::kMemlog;
}
