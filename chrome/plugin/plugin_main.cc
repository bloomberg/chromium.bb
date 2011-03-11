// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <objbase.h>
#include <windows.h>
#endif

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/default_plugin.h"
#include "chrome/common/hi_res_timer_manager.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/main_function_params.h"
#include "chrome/plugin/plugin_thread.h"
#include "content/common/child_process.h"
#include "ui/base/system_monitor/system_monitor.h"

#if defined(OS_WIN)
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/global_descriptors_posix.h"
#include "ipc/ipc_descriptors.h"
#endif

#if defined(OS_MACOSX)
// Removes our Carbon library interposing from the environment so that it
// doesn't carry into any processes that plugins might start.
void TrimInterposeEnvironment();

// Initializes the global Cocoa application object.
void InitializeChromeApplication();
#elif defined(OS_LINUX)
// Work around an unimplemented instruction in 64-bit Flash.
void WorkaroundFlashLAHF();
#endif

#if defined(OS_WIN)
// This function is provided so that the built-in flash can lock down the
// sandbox by calling DelayedLowerToken(0).
extern "C" DWORD __declspec(dllexport) __stdcall DelayedLowerToken(void* ts) {
  // s_ts is only set the first time the function is called, which happens
  // in PluginMain.
  static sandbox::TargetServices* s_ts =
      reinterpret_cast<sandbox::TargetServices*>(ts);
  if (ts)
    return 0;
  s_ts->LowerToken();
  return 1;
};

// Returns true if the plugin to be loaded is the internal flash.
bool IsPluginBuiltInFlash(const CommandLine& cmd_line) {
  FilePath path =  cmd_line.GetSwitchValuePath(switches::kPluginPath);
  return (path.BaseName() == FilePath(L"gcswf32.dll"));
}

// Before we lock down the flash sandbox, we need to activate
// the IME machinery. After lock down it seems it is unable
// to start. Note that we leak the IME context on purpose.
int PreloadIMEForFlash() {
  HIMC imc = ::ImmCreateContext();
  if (!imc)
    return 0;
  if (::ImmGetOpenStatus(imc))
    return 1;
  return 2;
}

#endif

// main() routine for running as the plugin process.
int PluginMain(const MainFunctionParams& parameters) {
  // The main thread of the plugin services UI.
#if defined(OS_MACOSX)
#if !defined(__LP64__)
  TrimInterposeEnvironment();
#endif
  InitializeChromeApplication();
#endif
  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  base::PlatformThread::SetName("CrPluginMain");

  ui::SystemMonitor system_monitor;
  HighResolutionTimerManager high_resolution_timer_manager;

  const CommandLine& parsed_command_line = parameters.command_line_;

#if defined(OS_LINUX)

#if defined(ARCH_CPU_64_BITS)
  WorkaroundFlashLAHF();
#endif

#elif defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();

  CoInitialize(NULL);
  DVLOG(1) << "Started plugin with "
           << parsed_command_line.command_line_string();

  HMODULE sandbox_test_module = NULL;
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);

  if (target_services && !no_sandbox) {
    // The command line might specify a test plugin to load.
    if (parsed_command_line.HasSwitch(switches::kTestSandbox)) {
      std::wstring test_plugin_name =
          parsed_command_line.GetSwitchValueNative(switches::kTestSandbox);
      sandbox_test_module = LoadLibrary(test_plugin_name.c_str());
      DCHECK(sandbox_test_module);
    }
  }
#endif
  if (parsed_command_line.HasSwitch(switches::kPluginStartupDialog)) {
    ChildProcess::WaitForDebugger("Plugin");
  }

  {
    ChildProcess plugin_process;
    plugin_process.set_main_thread(new PluginThread());
#if defined(OS_WIN)
    if (!no_sandbox && target_services) {
      // We are sandboxing the plugin. If it is a generic plug-in, we lock down
      // the sandbox right away, but if it is the built-in flash we let flash
      // start elevated and it will call DelayedLowerToken(0) when it's ready.
      if (IsPluginBuiltInFlash(parsed_command_line)) {
        DVLOG(1) << "Sandboxing flash";
        if (!PreloadIMEForFlash())
          DVLOG(1) << "IME preload failed";
        DelayedLowerToken(target_services);
      } else {
        target_services->LowerToken();
      }
    }
    if (sandbox_test_module) {
      RunRendererTests run_security_tests =
          reinterpret_cast<RunPluginTests>(GetProcAddress(sandbox_test_module,
                                                          kPluginTestCall));
      DCHECK(run_security_tests);
      if (run_security_tests) {
        int test_count = 0;
        DVLOG(1) << "Running plugin security tests";
        BOOL result = run_security_tests(&test_count);
        DCHECK(result) << "Test number " << test_count << " has failed.";
        // If we are in release mode, crash or debug the process.
        if (!result) {
          __debugbreak();
          _exit(1);
        }
      }

      FreeLibrary(sandbox_test_module);
    }
#endif

    chrome::RegisterInternalDefaultPlugin();

    MessageLoop::current()->Run();
  }

#if defined(OS_WIN)
  CoUninitialize();
#endif

  return 0;
}
