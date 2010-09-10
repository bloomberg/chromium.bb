// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_main.h"

#include "app/x11_util.h"
#include "app/x11_util_internal.h"
#include "base/command_line.h"
#include "base/debug_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_main_gtk.h"
#include "chrome/browser/browser_main_win.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/result_codes.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

namespace {

// Indicates that we're currently responding to an IO error (by shutting down).
bool g_in_x11_io_error_handler = false;

int BrowserX11ErrorHandler(Display* d, XErrorEvent* error) {
  if (!g_in_x11_io_error_handler)
    LOG(ERROR) << x11_util::GetErrorEventDescription(error);
  return 0;
}

int BrowserX11IOErrorHandler(Display* d) {
  // If there's an IO error it likely means the X server has gone away
  if (!g_in_x11_io_error_handler) {
    g_in_x11_io_error_handler = true;
    LOG(ERROR) << "X IO Error detected";
    BrowserList::WindowsSessionEnding();
  }

  return 0;
}

}  // namespace

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

void SetBrowserX11ErrorHandlers() {
  // Set up error handlers to make sure profile gets written if X server
  // goes away.
  x11_util::SetX11ErrorHandlers(
      BrowserX11ErrorHandler,
      BrowserX11IOErrorHandler);
}
