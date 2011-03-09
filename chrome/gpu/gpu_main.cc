// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "app/win/scoped_com_initializer.h"
#include "base/environment.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/main_function_params.h"
#include "chrome/gpu/gpu_config.h"
#include "chrome/gpu/gpu_process.h"
#include "chrome/gpu/gpu_thread.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/common/chrome_application_mac.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  base::Time start_time = base::Time::Now();

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kGpuStartupDialog)) {
    ChildProcess::WaitForDebugger("Gpu");
  }

#if defined(OS_MACOSX)
  chrome_application_mac::RegisterCrApp();
#endif

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  base::PlatformThread::SetName("CrGpuMain");

  if (!command_line.HasSwitch(switches::kSingleProcess)) {
#if defined(OS_WIN)
    // Prevent Windows from displaying a modal dialog on failures like not being
    // able to load a DLL.
    SetErrorMode(
        SEM_FAILCRITICALERRORS |
        SEM_NOGPFAULTERRORBOX |
        SEM_NOOPENFILEERRORBOX);
#elif defined(USE_X11)
    ui::SetDefaultX11ErrorHandlers();
#endif
  }

  app::win::ScopedCOMInitializer com_initializer;

  // We can not tolerate early returns from this code, because the
  // detection of early return of a child process is implemented using
  // an IPC channel error. If the IPC channel is not fully set up
  // between the browser and GPU process, and the GPU process crashes
  // or exits early, the browser process will never detect it.  For
  // this reason we defer all work related to the GPU until receiving
  // the GpuMsg_Initialize message from the browser.
  GpuProcess gpu_process;

  GpuThread* gpu_thread =
#if defined(OS_WIN)
      new GpuThread(parameters.sandbox_info_.TargetServices());
#else
      new GpuThread;
#endif

  gpu_thread->Init(start_time);

  gpu_process.set_main_thread(gpu_thread);

  main_message_loop.Run();

  gpu_thread->StopWatchdog();

  return 0;
}
