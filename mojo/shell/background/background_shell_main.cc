// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/background/background_shell_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/process/launch.h"
#include "mojo/shell/runner/common/switches.h"
#include "mojo/shell/runner/host/child_process.h"
#include "mojo/shell/runner/init.h"

namespace mojo {
namespace shell {
namespace {

int RunChildProcess() {
  base::AtExitManager at_exit;
  InitializeLogging();
  WaitForDebuggerIfNecessary();
#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
  return ChildProcessMain();
}

}  // namespace
}  // shell
}  // mojo

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kChildProcess)) {
    return mojo::shell::RunChildProcess();
  }
  // Reset CommandLine as most likely main() is going to use CommandLine too
  // and expect to be able to initialize it.
  base::CommandLine::Reset();
  return MasterProcessMain(argc, argv);
}
