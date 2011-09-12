// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/nacl/nacl_broker_listener.h"
#include "chrome/nacl/nacl_listener.h"
#include "chrome/nacl/nacl_main_platform_delegate.h"
#include "content/app/startup_helper_win.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/main_function_params.h"
#include "content/common/sandbox_init_wrapper.h"
#include "content/common/sandbox_policy.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_types.h"

extern int NaClMain(const MainFunctionParams&);

// main() routine for the NaCl broker process.
// This is necessary for supporting NaCl in Chrome on Win64.
int NaClBrokerMain(const MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line_;

  MessageLoopForIO main_message_loop;
  base::PlatformThread::SetName("CrNaClBrokerMain");

  base::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

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

  NaClBrokerListener listener;
  listener.Listen();

  return 0;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);

  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);

  wchar_t path[MAX_PATH];
  ::GetModuleFileNameW(NULL, path, MAX_PATH);
  InitCrashReporterWithDllPath(std::wstring(path));

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  // Copy what ContentMain() does.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  content::RegisterInvalidParamHandler();
  content::SetupCRT(command_line);

  // Initialize the sandbox for this process.
  SandboxInitWrapper sandbox_wrapper;
  sandbox_wrapper.SetServices(&sandbox_info);
  bool sandbox_initialized_ok =
      sandbox_wrapper.InitializeSandbox(command_line, process_type);
  // Die if the sandbox can't be enabled.
  CHECK(sandbox_initialized_ok) << "Error initializing sandbox for "
                                << process_type;
  MainFunctionParams main_params(command_line, sandbox_wrapper, NULL);

  if (process_type == switches::kNaClLoaderProcess)
    return NaClMain(main_params);

  if (process_type == switches::kNaClBrokerProcess)
    return NaClBrokerMain(main_params);

  CHECK(false) << "Unknown NaCl 64 process.";
  return -1;
}
