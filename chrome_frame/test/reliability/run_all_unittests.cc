// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/reliability/reliability_test_suite.h"

#include "base/command_line.h"
#include "chrome/common/chrome_paths.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"

static const char kRegisterDllFlag[] = "register";

int main(int argc, char **argv) {

  // If --register is passed, then we need to ensure that Chrome Frame is
  // registered before starting up the reliability tests.
  CommandLine::Init(argc, argv);
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  DCHECK(cmd_line);

  // We create this slightly early as it is the one who instantiates THE
  // AtExitManager which some of the other stuff below relies on.
  ReliabilityTestSuite test_suite(argc, argv);

  SetConfigBool(kChromeFrameHeadlessMode, true);
  base::ProcessHandle crash_service = chrome_frame_test::StartCrashService();

  int result = -1;
  if (cmd_line->HasSwitch(kRegisterDllFlag)) {
    std::wstring dll_path = cmd_line->GetSwitchValueNative(kRegisterDllFlag);

    // Run() must be called within the scope of the ScopedChromeFrameRegistrar
    // to ensure that the correct DLL remains registered during the tests.
    ScopedChromeFrameRegistrar scoped_chrome_frame_registrar(
        dll_path, ScopedChromeFrameRegistrar::SYSTEM_LEVEL);
    result = test_suite.Run();
  } else {
    result = test_suite.Run();
  }

  DeleteConfigValue(kChromeFrameHeadlessMode);
  if (crash_service)
    base::KillProcess(crash_service, 0, false);

  return result;
}

