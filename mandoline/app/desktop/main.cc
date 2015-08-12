// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "mandoline/app/desktop/launcher_process.h"
#include "mojo/runner/child_process.h"
#include "mojo/runner/init.h"
#include "mojo/runner/switches.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::AtExitManager at_exit;
  mojo::runner::InitializeLogging();
  mojo::runner::WaitForDebuggerIfNecessary();

  if (command_line.HasSwitch(switches::kChildProcess))
    return mojo::runner::ChildProcessMain();

  return mandoline::LauncherProcessMain(argc, argv);
}
