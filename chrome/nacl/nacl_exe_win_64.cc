// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/strings/string_util.h"
#include "base/timer/hi_res_timer_manager.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/app/chrome_breakpad_client.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/logging_chrome.h"
#include "components/breakpad/breakpad_client.h"
#include "components/nacl/broker/nacl_broker_listener.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/nacl_main_platform_delegate.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "sandbox/win/src/sandbox_types.h"

extern int NaClMain(const content::MainFunctionParams&);

namespace {

base::LazyInstance<chrome::ChromeBreakpadClient>::Leaky
    g_chrome_breakpad_client = LAZY_INSTANCE_INITIALIZER;

} // namespace

// main() routine for the NaCl broker process.
// This is necessary for supporting NaCl in Chrome on Win64.
int NaClBrokerMain(const content::MainFunctionParams& parameters) {
  const CommandLine& parsed_command_line = parameters.command_line;

  base::MessageLoopForIO main_message_loop;
  base::PlatformThread::SetName("CrNaClBrokerMain");

  scoped_ptr<base::PowerMonitorSource> power_monitor_source(
      new base::PowerMonitorDeviceSource());
  base::PowerMonitor power_monitor(power_monitor_source.Pass());
  base::HighResolutionTimerManager hi_res_timer_manager;

  NaClBrokerListener listener;
  listener.Listen();

  return 0;
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);

  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);

  breakpad::SetBreakpadClient(g_chrome_breakpad_client.Pointer());
  InitCrashReporter();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  // Copy what ContentMain() does.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  content::RegisterInvalidParamHandler();
  content::SetupCRT(command_line);
  // Route stdio to parent console (if any) or create one.
  if (command_line.HasSwitch(switches::kEnableLogging))
    base::RouteStdioToConsole();

  // Initialize the sandbox for this process.
  bool sandbox_initialized_ok = content::InitializeSandbox(&sandbox_info);
  // Die if the sandbox can't be enabled.
  CHECK(sandbox_initialized_ok) << "Error initializing sandbox for "
                                << process_type;
  content::MainFunctionParams main_params(command_line);
  main_params.sandbox_info = &sandbox_info;

  if (process_type == switches::kNaClLoaderProcess)
    return NaClMain(main_params);

  if (process_type == switches::kNaClBrokerProcess)
    return NaClBrokerMain(main_params);

  CHECK(false) << "Unknown NaCl 64 process.";
  return -1;
}
