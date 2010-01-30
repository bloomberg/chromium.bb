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
#include "chrome/common/result_codes.h"
#if defined(OS_WIN)
#include "chrome/nacl/broker_thread.h"
#endif
#include "chrome/nacl/nacl_thread.h"

#ifdef _WIN64

sandbox::BrokerServices* g_broker_services = NULL;

// main() routine for the NaCl broker process.
// This is necessary for supporting NaCl in Chrome on Win64.
int NaClBrokerMain(const MainFunctionParams& parameters) {
  // The main thread of the broker.
  MessageLoopForIO main_message_loop;
  std::wstring app_name = chrome::kNaClAppName;
  PlatformThread::SetName(WideToASCII(app_name + L"_NaClBrokerMain").c_str());

  SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  const CommandLine& parsed_command_line = parameters.command_line_;

  DLOG(INFO) << "Started NaCL broker with " <<
      parsed_command_line.command_line_string();

  // NOTE: this code is duplicated from browser_main.cc
  // IMPORTANT: This piece of code needs to run as early as possible in the
  // process because it will initialize the sandbox broker, which requires the
  // process to swap its window station. During this time all the UI will be
  // broken. This has to run before threads and windows are created.
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services) {
    g_broker_services = broker_services;
    if (!parsed_command_line.HasSwitch(switches::kNoSandbox)) {
      bool use_winsta = !parsed_command_line.HasSwitch(
          switches::kDisableAltWinstation);
      // Precreate the desktop and window station used by the renderers.
      sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
      sandbox::ResultCode result = policy->CreateAlternateDesktop(use_winsta);
      CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
      policy->Release();
    }
  }

  {
    ChildProcess broker_process;
    broker_process.set_main_thread(new NaClBrokerThread());
    MessageLoop::current()->Run();
  }

  return 0;
}
#else
int NaClBrokerMain(const MainFunctionParams& parameters) {
  return ResultCodes::BAD_PROCESS_TYPE;
}
#endif  // _WIN64

// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleNaClTestParameters(const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kNaClStartupDialog)) {
    ChildProcess::WaitForDebugger(L"NativeClient");
  }
}

// Launch the NaCl child process in its own thread.
#if defined (OS_WIN)
static void LaunchNaClChildProcess(bool no_sandbox,
                                   sandbox::TargetServices* target_services) {
  ChildProcess nacl_process;
  nacl_process.set_main_thread(new NaClThread());
  if (!no_sandbox && target_services)
    target_services->LowerToken();
  MessageLoop::current()->Run();
}
#elif defined(OS_MACOSX)
static void LaunchNaClChildProcess() {
  ChildProcess nacl_process;
  nacl_process.set_main_thread(new NaClThread());
  MessageLoop::current()->Run();
}
#endif

// main() routine for the NaCl loader process.
int NaClMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;

  // This function allows pausing execution using the --nacl-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleNaClTestParameters(parsed_command_line);

  // The main thread of the plugin services IO.
  MessageLoopForIO main_message_loop;
  // NaCl code runs in a different binary on Win64.
#ifdef _WIN64
  std::wstring app_name = chrome::kNaClAppName;
#else
  std::wstring app_name = chrome::kBrowserAppName;
#endif
  PlatformThread::SetName(WideToASCII(app_name + L"_NaClMain").c_str());

  SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();

  DLOG(INFO) << "Started NaCl loader with " <<
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
  LaunchNaClChildProcess(no_sandbox, target_services);

#elif defined(OS_MACOSX)
  LaunchNaClChildProcess();

#else
  NOTIMPLEMENTED() << " not implemented startup, plugin startup dialog etc.";
#endif

  return 0;
}


