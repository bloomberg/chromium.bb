// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/hi_res_timer_manager.h"
#include "chrome/worker/worker_thread.h"
#include "content/common/child_process.h"
#include "content/common/main_function_params.h"
#include "ui/base/system_monitor/system_monitor.h"

#if defined(OS_WIN)
#include "content/common/sandbox_init_wrapper.h"
#include "sandbox/src/sandbox.h"
#endif

// Mainline routine for running as the worker process.
int WorkerMain(const MainFunctionParams& parameters) {
  // The main message loop of the worker process.
  MessageLoop main_message_loop;
  base::PlatformThread::SetName("CrWorkerMain");

  ui::SystemMonitor system_monitor;
  HighResolutionTimerManager hi_res_timer_manager;

  ChildProcess worker_process;
  worker_process.set_main_thread(new WorkerThread());
#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();
  if (!target_services)
    return false;

  // Cause advapi32 to load before the sandbox is turned on.
  unsigned int dummy_rand;
  rand_s(&dummy_rand);

  target_services->LowerToken();
#endif

  const CommandLine& parsed_command_line = parameters.command_line_;
  if (parsed_command_line.HasSwitch(switches::kWaitForDebugger)) {
    ChildProcess::WaitForDebugger("Worker");
  }

  // Load the accelerator table from the browser executable and tell the
  // message loop to use it when translating messages.
  MessageLoop::current()->Run();

  return 0;
}
