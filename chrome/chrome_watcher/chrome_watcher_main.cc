// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging_win.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "base/template_util.h"
#include "components/browser_watcher/exit_code_watcher_win.h"
#include "components/browser_watcher/exit_funnel_win.h"
#include "components/browser_watcher/watcher_main_api_win.h"

namespace {

// Use the same log facility as Chrome for convenience.
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeWatcherTraceProviderName = {
    0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

// The data shared from the main function to the console handler.
struct HandlerData {
  HandlerData() : handler_done(true /* manual_reset */,
                               false /* initially_signaled */) {
  }

  base::WaitableEvent handler_done;
  base::ProcessHandle process;
  const base::char16* registry_path;
};

HandlerData *g_handler_data = nullptr;

BOOL CALLBACK ConsoleCtrlHandler(DWORD ctl_type) {
  if (g_handler_data && ctl_type == CTRL_LOGOFF_EVENT) {
    // Record the watcher logoff event in the browser's exit funnel.
    browser_watcher::ExitFunnel funnel;
    funnel.Init(g_handler_data->registry_path, g_handler_data->process);
    funnel.RecordEvent(L"WatcherLogoff");

    // Release the main function.
    g_handler_data->handler_done.Signal();
  }

  // Fall through to the next handler in turn.
  return FALSE;
}

}  // namespace

// The main entry point to the watcher, declared as extern "C" to avoid name
// mangling.
extern "C" int WatcherMain(const base::char16* registry_path) {
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  // Initialize the commandline singleton from the environment.
  base::CommandLine::Init(0, nullptr);

  logging::LogEventProvider::Initialize(kChromeWatcherTraceProviderName);

  // Arrange to be shut down as late as possible, as we want to outlive
  // chrome.exe in order to report its exit status.
  // TODO(siggi): Does this (windowless) process need to register a console
  //     handler too, in order to get notice of logoff events?
  ::SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);

  browser_watcher::ExitCodeWatcher exit_code_watcher(registry_path);
  int ret = 1;
  // Attempt to wait on our parent process, and record its exit status.
  if (exit_code_watcher.ParseArguments(
        *base::CommandLine::ForCurrentProcess())) {
    // Set up a console control handler, and provide it the data it needs
    // to record into the browser's exit funnel.
    HandlerData data;
    data.process = exit_code_watcher.process().Handle();
    data.registry_path = registry_path;
    g_handler_data = &data;
    ::SetConsoleCtrlHandler(&ConsoleCtrlHandler, TRUE /* Add */);

    // Wait on the process.
    exit_code_watcher.WaitForExit();

    browser_watcher::ExitFunnel funnel;
    funnel.Init(registry_path, exit_code_watcher.process().Handle());
    funnel.RecordEvent(L"BrowserExit");

    // Chrome/content exit codes are currently in the range of 0-28.
    // Anything outside this range is strange, so watch harder.
    int exit_code = exit_code_watcher.exit_code();
    if (exit_code < 0 || exit_code > 28) {
      // Wait for a max of 30 seconds to see whether we get notified of logoff.
      data.handler_done.TimedWait(base::TimeDelta::FromSeconds(30));
    }

    // Remove the console control handler.
    // There is a potential race here, where the handler might be executing
    // already as we fall through here. Hopefully SetConsoleCtrlHandler is
    // synchronizing against handler execution, for there's no other way to
    // close the race. Thankfully we'll just get snuffed out, as this is logoff.
    ::SetConsoleCtrlHandler(&ConsoleCtrlHandler, FALSE /* Add */);
    g_handler_data = nullptr;

    ret = 0;
  }

  // Wind logging down.
  logging::LogEventProvider::Uninitialize();

  return ret;
}

static_assert(base::is_same<decltype(&WatcherMain),
                            browser_watcher::WatcherMainFunction>::value,
              "WatcherMain() has wrong type");
