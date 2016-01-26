// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/standalone/desktop/main_helper.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/shell/runner/host/child_process.h"
#include "mojo/shell/runner/host/switches.h"
#include "mojo/shell/runner/init.h"
#include "mojo/shell/standalone/desktop/launcher_process.h"

#if defined(OS_WIN)
#include <windows.h>
#elif (OS_POSIX)
#include <unistd.h>
#endif

namespace mojo {
namespace shell {

int StandaloneShellMain(int argc,
                        char** argv,
                        const GURL& mojo_url,
                        const base::Closure& callback) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::AtExitManager at_exit;
  InitializeLogging();
  WaitForDebuggerIfNecessary();

#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif

  if (command_line.HasSwitch(switches::kChildProcess))
    return ChildProcessMain();

  return LauncherProcessMain(mojo_url, callback);
}

}  // namespace shell
}  // namespace mojo
