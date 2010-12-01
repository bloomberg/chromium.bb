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
#include "gfx/gtk_util.h"
#endif

namespace {

// 1% per watchdog trial group.
const int kFieldTrialSize = 1;

// 5 - 20 seconds timeout.
const int kMinGpuTimeout = 5;
const int kMaxGpuTimeout = 20;

}  // namespace

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
  // The X11 port of the command buffer code assumes it can access the X
  // display via the macro GDK_DISPLAY(), which implies that Gtk has been
  // initialized. This code was taken from PluginThread. TODO(kbr):
  // rethink whether initializing Gtk is really necessary or whether we
  // should just send a raw display connection down to the GPUProcessor.
  g_thread_init(NULL);
  gfx::GtkInitFromCommandLine(command_line);
#endif

  // Load the GL implementation and locate the bindings before starting the GPU
  // watchdog because this can take a lot of time and the GPU watchdog might
  // terminate the GPU process.
  if (!gfx::GLContext::InitializeOneOff())
    return EXIT_FAILURE;

  // Do this soon before running the message loop so accurate
  // initialization time is recorded in the GPU info. Don't do it before
  // starting the watchdog thread since it can take a significant amount of
  // time to collect GPU information in GpuThread::Init.
  GpuProcess gpu_process;
  GpuThread* gpu_thread = new GpuThread;
  gpu_thread->Init(start_time);
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

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  scoped_refptr<GpuWatchdogThread> watchdog_thread;
  if (enable_watchdog) {
    watchdog_thread = new GpuWatchdogThread(watchdog_timeout * 1000);
    watchdog_thread->Start();
  }

  main_message_loop.Run();

  if (enable_watchdog)
    watchdog_thread->Stop();

  return 0;
}
