// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/environment.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_com_initializer.h"
#include "build/build_config.h"
#include "content/common/content_switches.h"
#include "content/common/gpu/gpu_config.h"
#include "content/common/main_function_params.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gl_switches.h"

#if defined(OS_MACOSX)
#include "content/common/chrome_application_mac.h"
#elif defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  base::Time start_time = base::Time::Now();

  const CommandLine& command_line = parameters.command_line_;
  if (command_line.HasSwitch(switches::kGpuStartupDialog)) {
    ChildProcess::WaitForDebugger("Gpu");
  }

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

  // Initialization of the OpenGL bindings may fail, in which case we
  // will need to tear down this process. However, we can not do so
  // safely until the IPC channel is set up, because the detection of
  // early return of a child process is implemented using an IPC
  // channel error. If the IPC channel is not fully set up between the
  // browser and GPU process, and the GPU process crashes or exits
  // early, the browser process will never detect it.  For this reason
  // we defer tearing down the GPU process until receiving the
  // GpuMsg_Initialize message from the browser.
  bool dead_on_arrival = false;

  // Load the GL implementation and locate the bindings before starting the GPU
  // watchdog because this can take a lot of time and the GPU watchdog might
  // terminate the GPU process.
  if (!gfx::GLSurface::InitializeOneOff()) {
    LOG(INFO) << "GLContext::InitializeOneOff failed";
    dead_on_arrival = true;
  }

  base::win::ScopedCOMInitializer com_initializer;

#if defined(OS_WIN)
  sandbox::TargetServices* target_services =
      parameters.sandbox_info_.TargetServices();
  // For windows, if the target_services interface is not zero, the process
  // is sandboxed and we must call LowerToken() before rendering untrusted
  // content.
  if (target_services)
    target_services->LowerToken();
#endif

#if defined(OS_MACOSX)
  chrome_application_mac::RegisterCrApp();
#endif

  MessageLoop::Type message_loop_type = MessageLoop::TYPE_UI;
#if defined(OS_WIN)
  // Unless we're running on desktop GL, we don't need a UI message
  // loop, so avoid its use to work around apparent problems with some
  // third-party software.
  message_loop_type = MessageLoop::TYPE_IO;
  if (command_line.HasSwitch(switches::kUseGL) &&
      command_line.GetSwitchValueASCII(switches::kUseGL) ==
          gfx::kGLImplementationDesktopName) {
      message_loop_type = MessageLoop::TYPE_UI;
  }
#endif

  MessageLoop main_message_loop(message_loop_type);
  base::PlatformThread::SetName("CrGpuMain");

  GpuProcess gpu_process;

  GpuChildThread* child_thread = new GpuChildThread(dead_on_arrival);

  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  main_message_loop.Run();

  child_thread->StopWatchdog();

  return 0;
}
