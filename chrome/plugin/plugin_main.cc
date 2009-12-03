// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "app/hi_res_timer_manager.h"
#include "app/system_monitor.h"
#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/plugin/plugin_thread.h"

#if defined(OS_WIN)
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"
#elif defined(OS_LINUX)
#include "base/global_descriptors_posix.h"
#include "ipc/ipc_descriptors.h"
#endif

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_MACOSX)
// Removes our Carbon library interposing from the environment so that it
// doesn't carry into any processes that plugins might start.
void TrimInterposeEnvironment();

// Initializes the global Cocoa application object.
void InitializeChromeApplication();
#endif  // OS_MACOSX

// main() routine for running as the plugin process.
int PluginMain(const MainFunctionParams& parameters) {
#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  // The main thread of the plugin services UI.
#if defined(OS_MACOSX)
  TrimInterposeEnvironment();
  InitializeChromeApplication();
#endif
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_PluginMain").c_str());

  SystemMonitor system_monitor;
  HighResolutionTimerManager high_resolution_timer_manager;

  const CommandLine& parsed_command_line = parameters.command_line_;

#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();

  CoInitialize(NULL);
  DLOG(INFO) << "Started plugin with " <<
    parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox) ||
                    !parsed_command_line.HasSwitch(switches::kSafePlugins);
  if (target_services && !no_sandbox) {
    // The command line might specify a test plugin to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_plugin_name =
          parsed_command_line.GetSwitchValue(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_plugin_name.c_str());
      DCHECK(sandbox_test_module);
    }
  }
#endif
  if (parsed_command_line.HasSwitch(switches::kPluginStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Plugin");
  }

  {
    ChildProcess plugin_process;
    plugin_process.set_main_thread(new PluginThread());
#if defined(OS_WIN)
    if (!no_sandbox && target_services)
      target_services->LowerToken();

    if (sandbox_test_module) {
      RunRendererTests run_security_tests =
          reinterpret_cast<RunPluginTests>(GetProcAddress(sandbox_test_module,
                                                          kPluginTestCall));
      DCHECK(run_security_tests);
      if (run_security_tests) {
        int test_count = 0;
        DLOG(INFO) << "Running plugin security tests";
        BOOL result = run_security_tests(&test_count);
        DCHECK(result) << "Test number " << test_count << " has failed.";
        // If we are in release mode, crash or debug the process.
        if (!result)
          __debugbreak();
      }

      FreeLibrary(sandbox_test_module);
    }
#endif

    MessageLoop::current()->Run();
  }

#if defined(OS_WIN)
  CoUninitialize();
#endif

  return 0;
}
