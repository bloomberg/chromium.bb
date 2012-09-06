// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/hi_res_timer_manager.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/platform_thread.h"
#include "content/common/child_process.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/utility/utility_thread_impl.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#endif

// Mainline routine for running as the utility process.
int UtilityMain(const content::MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrUtilityMain");

  base::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_LINUX)
  // Initialize the sandbox before any thread is created.
  content::InitializeSandbox();
#endif

  ChildProcess utility_process;
  utility_process.set_main_thread(new UtilityThreadImpl());

#if defined(OS_WIN)
  bool no_sandbox = parameters.command_line.HasSwitch(switches::kNoSandbox);
  if (!no_sandbox) {
    sandbox::TargetServices* target_services =
        parameters.sandbox_info->target_services;
    if (!target_services)
      return false;
    target_services->LowerToken();
  }
#endif

  MessageLoop::current()->Run();

  return 0;
}
