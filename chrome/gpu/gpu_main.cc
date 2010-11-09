// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "app/app_switches.h"
#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/environment.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/main_function_params.h"
#include "chrome/gpu/gpu_config.h"
#include "chrome/gpu/gpu_process.h"
#include "chrome/gpu/gpu_thread.h"
#include "chrome/gpu/gpu_watchdog_thread.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

#if defined(USE_X11)
#include "app/x11_util.h"
#include "app/x11_util_internal.h"
#endif


namespace {

// 1% per watchdog trial group.
const int kFieldTrialSize = 1;

// 5 - 20 seconds timeout.
const int kMinGpuTimeout = 5;
const int kMaxGpuTimeout = 20;

#if defined(USE_X11)
int GpuX11ErrorHandler(Display* d, XErrorEvent* error) {
  LOG(ERROR) << x11_util::GetErrorEventDescription(d, error);
  return 0;
}

void SetGpuX11ErrorHandlers() {
  // Set up the error handlers so that only general errors aren't fatal.
  x11_util::SetX11ErrorHandlers(GpuX11ErrorHandler, NULL);
}
#endif

}

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  base::Time start_time = base::Time::Now();

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

#if defined(OS_LINUX)
  // On Linux we exec ourselves from /proc/self/exe, but that makes the
  // process name that shows up in "ps" etc. for the GPU process show as
  // "exe" instead of "chrome" or something reasonable. Try to fix it.
  CommandLine::SetProcTitle();
#endif

  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kGpuStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Gpu");
  }

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  PlatformThread::SetName("CrGpuMain");

#if defined(OS_WIN)
  win_util::ScopedCOMInitializer com_initializer;
#endif

#if defined(USE_X11)
  SetGpuX11ErrorHandlers();
#endif

  // Load the GL implementation and locate the bindings before starting as
  // this can take a lot of time and the GPU watchdog might terminate the GPU
  // process.
  if (!gfx::GLContext::InitializeOneOff())
    return EXIT_FAILURE;

  GpuProcess gpu_process;
  GpuThread* gpu_thread = new GpuThread;
  gpu_process.set_main_thread(gpu_thread);

  // Only enable this experimental feaure for a subset of users.
  scoped_refptr<base::FieldTrial> watchdog_trial(
      new base::FieldTrial("GpuWatchdogTrial", 100));
  int watchdog_timeout = 0;
  for (int i = kMinGpuTimeout; i <= kMaxGpuTimeout; ++i) {
    int group = watchdog_trial->AppendGroup(StringPrintf("%dsecs", i),
                                            kFieldTrialSize);
    if (group == watchdog_trial->group()) {
      watchdog_timeout = i;
      break;
    }
  }

  scoped_ptr<base::Environment> env(base::Environment::Create());

  // In addition to disabling the watchdog if the command line switch is
  // present, disable it in two other cases. OSMesa is expected to run very
  // slowly.  Also disable the watchdog on valgrind because the code is expected
  // to run slowly in that case.
  bool enable_watchdog =
      watchdog_timeout != 0 &&
      !command_line.HasSwitch(switches::kDisableGpuWatchdog) &&
      gfx::GetGLImplementation() != gfx::kGLImplementationOSMesaGL &&
      !RunningOnValgrind();

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

// TODO(apatrick): Disable for this commit. I want to enable this feature with
// a simple single file change that can easily be reverted if need be without
// losing all the other features of the patch.
#if 1
  enable_watchdog = false;
#endif

  scoped_refptr<GpuWatchdogThread> watchdog_thread;
  if (enable_watchdog) {
    watchdog_thread = new GpuWatchdogThread(MessageLoop::current(),
                                            watchdog_timeout * 1000);
    watchdog_thread->Start();
  }

  // Do this immediately before running the message loop so the correct
  // initialization time is recorded in the GPU info.
  gpu_thread->Init(start_time);

  main_message_loop.Run();

  if (enable_watchdog)
    watchdog_thread->Stop();

  return 0;
}
