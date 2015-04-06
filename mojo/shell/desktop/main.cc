// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/desktop/launcher_process.h"
#include "mojo/shell/init.h"
#include "mojo/shell/switches.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  mojo::shell::InitializeLogging();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kChildProcess))
    return mojo::shell::ChildProcessMain();

  return mojo::shell::LauncherProcessMain(argc, argv);
}
