// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/hi_res_timer_manager.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/threading/platform_thread.h"
#include "content/common/child_process.h"
#include "content/plugin/plugin_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "content/public/common/injection_test_win.h"
#include "sandbox/win/src/sandbox.h"
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/posix/global_descriptors.h"
#include "ipc/ipc_descriptors.h"
#endif

namespace content {

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

  base::PowerMonitor power_monitor;
  HighResolutionTimerManager high_resolution_timer_manager;

  const CommandLine& parsed_command_line = parameters.command_line;

#if defined(OS_LINUX)

#if defined(ARCH_CPU_64_BITS)
  WorkaroundFlashLAHF();
#endif

#elif defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer;
#endif

  if (parsed_command_line.HasSwitch(switches::kPluginStartupDialog)) {
    ChildProcess::WaitForDebugger("Plugin");
  }

  {
    ChildProcess plugin_process;
    plugin_process.set_main_thread(new PluginThread());
    MessageLoop::current()->Run();
  }

  return 0;
}

}  // namespace content
