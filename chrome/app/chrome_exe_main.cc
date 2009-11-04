// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/win_util.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "chrome/common/result_codes.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox_factory.h"


int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  base::EnableTerminationOnHeapCorruption();

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  bool exit_now = true;
  // We restarted because of a previous crash. Ask user if we should relaunch.
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    if (exit_now)
      return ResultCodes::NORMAL_EXIT;
  }

  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  sandbox_info.broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!sandbox_info.broker_services)
    sandbox_info.target_services = sandbox::SandboxFactory::GetTargetServices();

  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    // Enforces strong DEP support. Vista uses the NXCOMPAT flag in the exe.
    sandbox::SetCurrentProcessDEP(sandbox::DEP_ENABLED);
  }

  // Load and launch the chrome dll. *Everything* happens inside.
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance, &sandbox_info);
  delete loader;

  return rc;
}
