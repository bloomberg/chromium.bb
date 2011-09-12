// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/client_util.h"
#include "content/app/startup_helper_win.h"
#include "content/common/result_codes.h"
#include "sandbox/src/sandbox_factory.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  bool exit_now = true;
  // We restarted because of a previous crash. Ask user if we should relaunch.
  if (ShowRestartDialogIfCrashed(&exit_now)) {
    if (exit_now)
      return content::RESULT_CODE_NORMAL_EXIT;
  }

  // Initialize the sandbox services.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;

  // Initialize the commandline singleton from the environment.
  CommandLine::Init(0, NULL);

  // Load and launch the chrome dll. *Everything* happens inside.
  MainDllLoader* loader = MakeMainDllLoader();
  int rc = loader->Launch(instance, &sandbox_info);
  loader->RelaunchChromeBrowserWithNewCommandLineIfNeeded();
  delete loader;

  return rc;
}
