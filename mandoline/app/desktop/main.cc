// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "mandoline/app/desktop/launcher_process.h"
#include "mojo/runner/child_process.h"
#include "mojo/runner/init.h"
#include "mojo/runner/native_application_support.h"
#include "mojo/runner/switches.h"
#include "mojo/shell/native_runner.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "mandoline/app/desktop/linux_sandbox.h"
#endif

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  using sandbox::syscall_broker::BrokerFilePermission;
  scoped_ptr<mandoline::LinuxSandbox> sandbox;
  if (command_line.HasSwitch(switches::kChildProcess) &&
      command_line.HasSwitch(switches::kEnableSandbox)) {
    std::vector<BrokerFilePermission> permissions =
        mandoline::LinuxSandbox::GetPermissions();
    permissions.push_back(BrokerFilePermission::ReadOnly(
        command_line.GetSwitchValueNative(switches::kChildProcess)));

    sandbox.reset(new mandoline::LinuxSandbox(permissions));
    sandbox->Warmup();
    sandbox->EngageNamespaceSandbox();
    sandbox->EngageSeccompSandbox();
    sandbox->Seal();
  }
#endif

  base::AtExitManager at_exit;
  mojo::runner::InitializeLogging();
  mojo::runner::WaitForDebuggerIfNecessary();

  if (command_line.HasSwitch(switches::kChildProcess))
    return mojo::runner::ChildProcessMain();

  return mandoline::LauncherProcessMain(argc, argv);
}
