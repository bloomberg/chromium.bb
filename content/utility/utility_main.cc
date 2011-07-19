// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/platform_thread.h"
#include "content/common/child_process.h"
#include "content/common/content_switches.h"
#include "content/common/hi_res_timer_manager.h"
#include "content/common/main_function_params.h"
#include "content/utility/utility_thread.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrUtilityMain");

  base::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  ChildProcess utility_process;
  utility_process.set_main_thread(new UtilityThread());

#if defined(OS_WIN)
  bool no_sandbox = parameters.command_line_.HasSwitch(switches::kNoSandbox);
  if (!no_sandbox) {
    sandbox::TargetServices* target_services =
        parameters.sandbox_info_.TargetServices();
    if (!target_services)
      return false;
    target_services->LowerToken();
  }
#endif

  MessageLoop::current()->Run();

  return 0;
}
