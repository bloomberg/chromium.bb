// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_implementation.h"
#include "base/message_loop.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
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

#if defined(USE_X11)
namespace {

int GpuX11ErrorHandler(Display* d, XErrorEvent* error) {
  LOG(ERROR) << x11_util::GetErrorEventDescription(d, error);
  return 0;
}

void SetGpuX11ErrorHandlers() {
  // Set up the error handlers so that only general errors aren't fatal.
  x11_util::SetX11ErrorHandlers(GpuX11ErrorHandler, NULL);
}

}
#endif

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
#if defined(USE_LINUX_BREAKPAD)
  // Needs to be called after we have chrome::DIR_USER_DATA.
  InitCrashReporter();
#endif

  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kGpuStartupDialog)) {
    ChildProcess::WaitForDebugger(L"Gpu");
  }

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  PlatformThread::SetName("CrGpuMain");

#if defined(OS_WIN)
  win_util::ScopedCOMInitializer com_initializer;
#elif defined(GPU_USE_GLX)
  gfx::InitializeGLBindings(gfx::kGLImplementationDesktopGL);
#endif

  GpuProcess gpu_process;
  gpu_process.set_main_thread(new GpuThread());

  scoped_refptr<GpuWatchdogThread> watchdog_thread(
      new GpuWatchdogThread(MessageLoop::current()));

  if (!command_line.HasSwitch(switches::kDisableGpuWatchdog))
    watchdog_thread->Start();

#if defined(USE_X11)
  SetGpuX11ErrorHandlers();
#endif

  main_message_loop.Run();

  if (!command_line.HasSwitch(switches::kDisableGpuWatchdog))
    watchdog_thread->Stop();

  return 0;
}

