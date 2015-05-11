// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/init.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/runner/switches.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace runner {

void InitializeLogging() {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // To view log output with IDs and timestamps use "adb logcat -v threadtime".
  logging::SetLogItems(false,   // Process ID
                       false,   // Thread ID
                       false,   // Timestamp
                       false);  // Tick count
}

void WaitForDebuggerIfNecessary() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWaitForDebugger)) {
    std::vector<std::string> apps_to_debug;
    base::SplitString(
        command_line->GetSwitchValueASCII(switches::kWaitForDebugger), ',',
        &apps_to_debug);
    std::string app = command_line->GetSwitchValueASCII(switches::kApp);
    if (app.empty())
      app = "launcher";  // If we're not in a child process look for "launcher".
    if (apps_to_debug.empty() || ContainsValue(apps_to_debug, app)) {
#if defined(OS_WIN)
      base::string16 appw = base::UTF8ToUTF16(app);
      MessageBox(NULL, appw.c_str(), appw.c_str(), MB_OK | MB_SETFOREGROUND);
#else
      LOG(ERROR) << app << " waiting for GDB. pid: " << getpid();
      base::debug::WaitForDebugger(60, true);
#endif
    }
  }
}

}  // namespace runner
}  // namespace mojo
