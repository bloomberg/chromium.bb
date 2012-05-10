// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/environment.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_com_initializer.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/gpu/gpu_process.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gl_switches.h"

#if defined(OS_WIN)
#include "content/common/gpu/media/dxva_video_decode_accelerator.h"
#include "sandbox/src/sandbox.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_LINUX)
#include "content/public/common/sandbox_init.h"
#endif

// Main function for starting the Gpu process.
int GpuMain(const content::MainFunctionParams& parameters) {
  base::Time start_time = base::Time::Now();

  const CommandLine& command_line = parameters.command_line;
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

  // Load and initialize the GL implementation and locate the GL entry points.
  content::GPUInfo gpu_info;
  if (gfx::GLSurface::InitializeOneOff()) {
    // Collect information about the GPU.
    if (!gpu_info_collector::CollectGraphicsInfo(&gpu_info)) {
      LOG(INFO) << "gpu_info_collector::CollectGraphicsInfo failed";
    }

#if defined(OS_LINUX)
    if (gpu_info.vendor_id == 0x10de &&  // NVIDIA
        gpu_info.driver_vendor == "NVIDIA") {
      base::ThreadRestrictions::AssertIOAllowed();
      if (access("/dev/nvidiactl", R_OK) != 0) {
        LOG(INFO) << "NVIDIA device file /dev/nvidiactl access denied";
        gpu_info.gpu_accessible = false;
        dead_on_arrival = true;
      }
    }
#endif

    // Set the GPU info even if it failed.
    content::GetContentClient()->SetGpuInfo(gpu_info);
  } else {
    LOG(INFO) << "gfx::GLSurface::InitializeOneOff failed";
    gpu_info.gpu_accessible = false;
    dead_on_arrival = true;
  }

  // Warm up the random subsystem, which needs to be done pre-sandbox on all
  // platforms.
  (void) base::RandUint64();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  content::InitializeSandbox();
#endif

  base::win::ScopedCOMInitializer com_initializer;

#if defined(OS_WIN)
  // Preload this DLL because the sandbox prevents it from loading.
  LoadLibrary(L"setupapi.dll");

  sandbox::TargetServices* target_services =
      parameters.sandbox_info->target_services;
  // Initialize H/W video decoding stuff which fails in the sandbox.
  DXVAVideoDecodeAccelerator::PreSandboxInitialization();
  // For windows, if the target_services interface is not zero, the process
  // is sandboxed and we must call LowerToken() before rendering untrusted
  // content.
  if (target_services)
    target_services->LowerToken();
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
#elif defined(OS_LINUX)
  message_loop_type = MessageLoop::TYPE_DEFAULT;
#endif

  MessageLoop main_message_loop(message_loop_type);
  base::PlatformThread::SetName("CrGpuMain");

  GpuProcess gpu_process;

  GpuChildThread* child_thread = new GpuChildThread(dead_on_arrival, gpu_info);

  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  main_message_loop.Run();

  child_thread->StopWatchdog();

  return 0;
}
