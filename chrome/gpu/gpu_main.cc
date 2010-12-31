// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "app/app_switches.h"
#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
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
#include "chrome/gpu/gpu_watchdog_thread.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/common/chrome_application_mac.h"
#include "chrome/common/sandbox_mac.h"
#endif

#if defined(OS_WIN)
#include "app/win_util.h"
#endif

#if defined(USE_X11)
#include "gfx/gtk_util.h"
#endif

const int kGpuTimeout = 10000;

namespace {

bool InitializeGpuSandbox() {
#if defined(OS_MACOSX)
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();
  SandboxInitWrapper sandbox_wrapper;
  return sandbox_wrapper.InitializeSandbox(*parsed_command_line,
                                           switches::kGpuProcess);
#else
  // TODO(port): Create GPU sandbox for linux and windows.
  return true;
#endif
}

}  // namespace

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  base::Time start_time = base::Time::Now();

#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kGpuStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Gpu");
  }

#if defined(OS_MACOSX)
  chrome_application_mac::RegisterCrApp();
#endif

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  base::PlatformThread::SetName("CrGpuMain");

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

  // Note that kNoSandbox will also disable the GPU sandbox.
  bool no_gpu_sandbox = command_line.HasSwitch(switches::kNoGpuSandbox);
  if (!no_gpu_sandbox) {
    if (!InitializeGpuSandbox()) {
      LOG(ERROR) << "Failed to initialize the GPU sandbox";
      return EXIT_FAILURE;
    }
  } else {
    LOG(ERROR) << "Running without GPU sandbox";
  }

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


  // In addition to disabling the watchdog if the command line switch is
  // present, disable it in two other cases. OSMesa is expected to run very
  // slowly.  Also disable the watchdog on valgrind because the code is expected
  // to run slowly in that case.
  bool enable_watchdog =
      !command_line.HasSwitch(switches::kDisableGpuWatchdog) &&
      gfx::GetGLImplementation() != gfx::kGLImplementationOSMesaGL &&
      !RunningOnValgrind();

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

  // Disable the watchdog for Windows. It tends to abort when the GPU process
  // is not hung but still taking a long time to do something. Instead, the
  // browser process displays a dialog when it notices that the child window
  // is hung giving the user an opportunity to terminate it. This is the
  // same mechanism used to abort hung plugins.
#if defined(OS_WIN)
  enable_watchdog = false;
#endif

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  scoped_refptr<GpuWatchdogThread> watchdog_thread;
  if (enable_watchdog) {
    watchdog_thread = new GpuWatchdogThread(kGpuTimeout);
    watchdog_thread->Start();
  }

  main_message_loop.Run();

  if (enable_watchdog)
    watchdog_thread->Stop();

  return 0;
}
