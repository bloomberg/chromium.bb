// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"

#include "base/command_line.h"
#include "base/debug_util.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/result_codes.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

void DidEndMainMessageLoop() {
}

void RecordBreakpadStatusUMA(MetricsService* metrics) {
#if defined(USE_LINUX_BREAKPAD)
  metrics->RecordBreakpadRegistration(IsCrashReporterEnabled());
#else
  metrics->RecordBreakpadRegistration(false);
#endif
  metrics->RecordBreakpadHasDebugger(DebugUtil::BeingDebugged());
}

void WarnAboutMinimumSystemRequirements() {
  // Nothing to warn about on GTK right now.
}

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return ResultCodes::NORMAL_EXIT;
}

int HandleIconsCommands(const CommandLine &parsed_command_line) {
  return 0;
}

bool CheckMachineLevelInstall() {
  return false;
}

void PrepareRestartOnCrashEnviroment(const CommandLine &parsed_command_line) {
}
