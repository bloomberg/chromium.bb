// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include "app/win_util.h"
#endif
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/system_monitor.h"
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
#elif defined(OS_MACOSX)
#include "chrome/common/plugin_carbon_interpose_constants_mac.h"
#endif

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_MACOSX)
// Removes our Carbon library interposing from the environment so that it
// doesn't carry into any processes that plugins might start.
static void TrimInterposeEnvironment() {
  const char* interpose_list =
      getenv(plugin_interpose_strings::kDYLDInsertLibrariesKey);
  if (!interpose_list) {
    NOTREACHED() << "No interposing libraries set";
    return;
  }

  // The list is a :-separated list of paths. Because we append our interpose
  // library just before forking in plugin_process_host.cc, the only cases we
  // need to handle are:
  // 1) The whole string is "<kInterposeLibraryPath>", so just clear it, or
  // 2) ":<kInterposeLibraryPath>" is the end of the string, so trim and re-set.
  int suffix_offset = strlen(interpose_list) -
      strlen(plugin_interpose_strings::kInterposeLibraryPath);
  if (suffix_offset == 0 &&
      strcmp(interpose_list,
             plugin_interpose_strings::kInterposeLibraryPath) == 0) {
    unsetenv(plugin_interpose_strings::kDYLDInsertLibrariesKey);
  } else if (suffix_offset > 0 && interpose_list[suffix_offset - 1] == ':' &&
             strcmp(interpose_list + suffix_offset,
                    plugin_interpose_strings::kInterposeLibraryPath) == 0) {
    std::string trimmed_list =
        std::string(interpose_list).substr(0, suffix_offset - 1);
    setenv(plugin_interpose_strings::kDYLDInsertLibrariesKey,
           trimmed_list.c_str(), 1);
  } else {
    NOTREACHED() << "Missing Carbon interposing library";
  }
}
#endif  // OS_MACOSX

// main() routine for running as the plugin process.
int PluginMain(const MainFunctionParams& parameters) {
#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  // The main thread of the plugin services UI.
#if defined(OS_MACOSX)
  // For Mac NPAPI plugins, we don't want a MessageLoop::TYPE_UI because
  // that will cause events to be dispatched via the Cocoa responder chain.
  // If the plugin creates its own windows with Carbon APIs (for example,
  // full screen mode in Flash), those windows would not receive events.
  // Instead, WebPluginDelegateImpl::OnNullEvent will dispatch any pending
  // system events directly to the plugin.
  MessageLoop main_message_loop(MessageLoop::TYPE_DEFAULT);
#else
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
#endif
  std::wstring app_name = chrome::kBrowserAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_PluginMain").c_str());

  // Initialize the SystemMonitor
  base::SystemMonitor::Start();

#if defined(OS_MACOSX)
  TrimInterposeEnvironment();
#endif

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
