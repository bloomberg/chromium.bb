// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"
#endif

#include "app/hi_res_timer_manager.h"
#include "app/system_monitor.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/nacl/nacl_thread.h"

// main() routine for running as the sel_ldr process.
int NaClMain(const MainFunctionParams& parameters) {
  // The main thread of the plugin services IO.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_NaClMain").c_str());

  SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_WIN)
  const CommandLine& parsed_command_line = parameters.command_line_;

  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();

  DLOG(INFO) << "Started plugin with " <<
      parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  if (target_services && !no_sandbox) {
    // The command line might specify a test plugin to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_plugin_name =
          parsed_command_line.GetSwitchValue(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_plugin_name.c_str());
      DCHECK(sandbox_test_module);
    }
  }

#else
  NOTIMPLEMENTED() << " non-windows startup, plugin startup dialog etc.";
#endif

  {
    ChildProcess nacl_process;
    nacl_process.set_main_thread(new NaClThread());
#if defined(OS_WIN)
    if (!no_sandbox && target_services)
      target_services->LowerToken();
#endif

    MessageLoop::current()->Run();
  }

  return 0;
}


