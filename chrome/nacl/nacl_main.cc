// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/result_codes.h"
#include "chrome/common/sandbox_policy.h"
#include "chrome/nacl/nacl_main_platform_delegate.h"
#include "chrome/nacl/nacl_thread.h"
#include "content/common/child_process.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/main_function_params.h"
#include "ui/base/system_monitor/system_monitor.h"

#if defined(OS_WIN)
#include "chrome/nacl/broker_thread.h"
#include "chrome/test/injection_test_dll.h"
#include "sandbox/src/sandbox.h"
#endif

#ifdef _WIN64

// main() routine for the NaCl broker process.
// This is necessary for supporting NaCl in Chrome on Win64.
int NaClBrokerMain(const MainFunctionParams& parameters) {
  // The main thread of the broker.
  MessageLoopForIO main_message_loop;
  base::PlatformThread::SetName("CrNaClBrokerMain");

  ui::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  const CommandLine& parsed_command_line = parameters.command_line_;

  DVLOG(1) << "Started NaCL broker with "
           << parsed_command_line.command_line_string();

  // NOTE: this code is duplicated from browser_main.cc
  // IMPORTANT: This piece of code needs to run as early as possible in the
  // process because it will initialize the sandbox broker, which requires the
  // process to swap its window station. During this time all the UI will be
  // broken. This has to run before threads and windows are created.
  sandbox::BrokerServices* broker_services =
      parameters.sandbox_info_.BrokerServices();
  if (broker_services) {
    sandbox::InitBrokerServices(broker_services);
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
    ChildProcess::WaitForDebugger("NativeClient");
  }
}

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
  base::PlatformThread::SetName("CrNaClMain");

  ui::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  NaClMainPlatformDelegate platform(parameters);

  platform.PlatformInitialize();
  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);
  platform.InitSandboxTests(no_sandbox);

  if (!no_sandbox) {
    platform.EnableSandbox();
  }
  bool sandbox_test_result = platform.RunSandboxTests();

  if (sandbox_test_result) {
    ChildProcess nacl_process;
    bool debug = parsed_command_line.HasSwitch(switches::kEnableNaClDebug);
    nacl_process.set_main_thread(new NaClThread(debug));
    MessageLoop::current()->Run();
  } else {
    // This indirectly prevents the test-harness-success-cookie from being set,
    // as a way of communicating test failure, because the nexe won't reply.
    // TODO(jvoung): find a better way to indicate failure that doesn't
    // require waiting for a timeout.
    VLOG(1) << "Sandbox test failed: Not launching NaCl process";
  }
#else
  NOTIMPLEMENTED() << " not implemented startup, plugin startup dialog etc.";
#endif

  platform.PlatformUninitialize();
  return 0;
}
