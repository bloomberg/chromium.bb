// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "content/child/child_process.h"
#include "content/common/sandbox_linux.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/sandbox_init.h"
#include "content/utility/utility_thread_impl.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#endif

namespace content {

// Mainline routine for running as the utility process.
int UtilityMain(const MainFunctionParams& parameters) {
  // The main message loop of the utility process.
  base::MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrUtilityMain");

#if defined(OS_LINUX)
  // Initialize the sandbox before any thread is created.
  LinuxSandbox::InitializeSandbox();
#endif

  ChildProcess utility_process;
  utility_process.set_main_thread(new UtilityThreadImpl());

  base::HighResolutionTimerManager hi_res_timer_manager;

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

  base::MessageLoop::current()->Run();

  return 0;
}

}  // namespace content
