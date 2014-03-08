// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace base {

#if !defined(OS_ANDROID)
ProcessHandle SpawnMultiProcessTestChild(
    const std::string& procname,
    const CommandLine& base_command_line,
    const LaunchOptions& options,
    bool debug_on_start) {
  CommandLine command_line(base_command_line);
  // TODO(viettrungluu): See comment above |MakeCmdLine()| in the header file.
  // This is a temporary hack, since |MakeCmdLine()| has to provide a full
  // command line.
  if (!command_line.HasSwitch(switches::kTestChildProcess)) {
    command_line.AppendSwitchASCII(switches::kTestChildProcess, procname);
    if (debug_on_start)
      command_line.AppendSwitch(switches::kDebugOnStart);
  }

  ProcessHandle handle = kNullProcessHandle;
  LaunchProcess(command_line, options, &handle);
  return handle;
}
#endif  // !defined(OS_ANDROID)

CommandLine GetMultiProcessTestChildBaseCommandLine() {
  return *CommandLine::ForCurrentProcess();
}

// MultiProcessTest ------------------------------------------------------------

MultiProcessTest::MultiProcessTest() {
}

ProcessHandle MultiProcessTest::SpawnChild(const std::string& procname,
                                           bool debug_on_start) {
  LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
#endif
  return SpawnChildWithOptions(procname, options, debug_on_start);
}

ProcessHandle MultiProcessTest::SpawnChildWithOptions(
    const std::string& procname,
    const LaunchOptions& options,
    bool debug_on_start) {
  return SpawnMultiProcessTestChild(procname,
                                    MakeCmdLine(procname, debug_on_start),
                                    options,
                                    debug_on_start);
}

CommandLine MultiProcessTest::MakeCmdLine(const std::string& procname,
                                          bool debug_on_start) {
  CommandLine command_line = GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII(switches::kTestChildProcess, procname);
  if (debug_on_start)
    command_line.AppendSwitch(switches::kDebugOnStart);
  return command_line;
}

}  // namespace base
