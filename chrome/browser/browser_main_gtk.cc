// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"

#include "base/command_line.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/common/result_codes.h"

namespace Platform {

void WillInitializeMainMessageLoop(const MainFunctionParams& parameters) {
}

void WillTerminate() {
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
  // TODO(port): http://crbug.com/21732
}

}  // namespace Platform

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return ResultCodes::NORMAL_EXIT;
}

bool DoUpgradeTasks(const CommandLine& command_line) {
  return ResultCodes::NORMAL_EXIT;
}

bool CheckForWin2000() {
  return false;
}

int HandleIconsCommands(const CommandLine &parsed_command_line) {
  return 0;
}

bool CheckMachineLevelInstall() {
  return false;
}

void PrepareRestartOnCrashEnviroment(const CommandLine &parsed_command_line) {
}
