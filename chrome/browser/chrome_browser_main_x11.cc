// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include "base/bind.h"
#include "base/debug/debugger.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/chrome_result_codes.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linuxish.h"
#endif

using content::BrowserThread;

namespace {

// Indicates that we're currently responding to an IO error (by shutting down).
bool g_in_x11_io_error_handler = false;

// Number of seconds to wait for UI thread to get an IO error if we get it on
// the background thread.
const int kWaitForUIThreadSeconds = 10;

int BrowserX11ErrorHandler(Display* d, XErrorEvent* error) {
  if (!g_in_x11_io_error_handler)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ui::LogErrorEventDescription, d, *error));
  return 0;
}


// This function is used to help us diagnose crash dumps that happen
// during the shutdown process.
NOINLINE void WaitingForUIThreadToHandleIOError() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(kWaitForUIThreadSeconds);
}

int BrowserX11IOErrorHandler(Display* d) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Wait for the UI thread (which has a different connection to the X server)
    // to get the error. We can't call shutdown from this thread without
    // tripping an error. Doing it through a function so that we'll be able
    // to see it in any crash dumps.
    WaitingForUIThreadToHandleIOError();
    return 0;
  }
  // If there's an IO error it likely means the X server has gone away
  if (!g_in_x11_io_error_handler) {
    g_in_x11_io_error_handler = true;
    LOG(ERROR) << "X IO Error detected";
    browser_shutdown::SetShuttingDownWithoutClosingBrowsers(true);
    browser::SessionEnding();
  }

  return 0;
}

}  // namespace

void RecordBreakpadStatusUMA(MetricsService* metrics) {
#if defined(USE_LINUX_BREAKPAD)
  metrics->RecordBreakpadRegistration(IsCrashReporterEnabled());
#else
  metrics->RecordBreakpadRegistration(false);
#endif
  metrics->RecordBreakpadHasDebugger(base::debug::BeingDebugged());
}

void WarnAboutMinimumSystemRequirements() {
  // Nothing to warn about on X11 right now.
}

void RecordBrowserStartupTime() {
  // Not implemented on X11 for now.
}

// From browser_main_win.h, stubs until we figure out the right thing...

int DoUninstallTasks(bool chrome_still_running) {
  return content::RESULT_CODE_NORMAL_EXIT;
}

void SetBrowserX11ErrorHandlers() {
  // Set up error handlers to make sure profile gets written if X server
  // goes away.
  ui::SetX11ErrorHandlers(BrowserX11ErrorHandler, BrowserX11IOErrorHandler);
}
