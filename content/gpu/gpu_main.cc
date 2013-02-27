// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_config.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/gpu/gpu_process.h"
#include "content/gpu/gpu_watchdog_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_switching_option.h"
#include "content/public/common/main_function_params.h"
#include "crypto/hmac.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "content/common/gpu/media/dxva_video_decode_accelerator.h"
#include "sandbox/win/src/sandbox.h"
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
#include "content/common/gpu/media/exynos_video_decode_accelerator.h"
#include "content/common/gpu/media/omx_video_decode_accelerator.h"
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
#include "content/common/gpu/media/vaapi_video_decode_accelerator.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_LINUX)
#include "content/public/common/sandbox_init.h"
#endif

const int kGpuTimeout = 10000;

namespace content {
namespace {
void WarmUpSandbox(const GPUInfo&, bool);
}

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  TRACE_EVENT0("gpu", "GpuMain");

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

  if (command_line.HasSwitch(switches::kSupportsDualGpus) &&
      command_line.HasSwitch(switches::kGpuSwitching)) {
    std::string option = command_line.GetSwitchValueASCII(
        switches::kGpuSwitching);
    if (option == switches::kGpuSwitchingOptionNameForceDiscrete)
      ui::GpuSwitchingManager::GetInstance()->ForceUseOfDiscreteGpu();
    else if (option == switches::kGpuSwitchingOptionNameForceIntegrated)
      ui::GpuSwitchingManager::GetInstance()->ForceUseOfIntegratedGpu();
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

  MessageLoop::Type message_loop_type = MessageLoop::TYPE_IO;
#if defined(OS_WIN)
  // Unless we're running on desktop GL, we don't need a UI message
  // loop, so avoid its use to work around apparent problems with some
  // third-party software.
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

  // In addition to disabling the watchdog if the command line switch is
  // present, disable the watchdog on valgrind because the code is expected
  // to run slowly in that case.
  bool enable_watchdog =
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuWatchdog) &&
      !RunningOnValgrind();

  // Disable the watchdog in debug builds because they tend to only be run by
  // developers who will not appreciate the watchdog killing the GPU process.
#ifndef NDEBUG
  enable_watchdog = false;
#endif

  bool delayed_watchdog_enable = false;

#if defined(OS_CHROMEOS)
  // Don't start watchdog immediately, to allow developers to switch to VT2 on
  // startup.
  delayed_watchdog_enable = true;
#endif

  scoped_refptr<GpuWatchdogThread> watchdog_thread;

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    watchdog_thread = new GpuWatchdogThread(kGpuTimeout);
    watchdog_thread->Start();
  }

  GPUInfo gpu_info;
  // Get vendor_id, device_id, driver_version from browser process through
  // commandline switches.
  DCHECK(command_line.HasSwitch(switches::kGpuVendorID) &&
         command_line.HasSwitch(switches::kGpuDeviceID) &&
         command_line.HasSwitch(switches::kGpuDriverVersion));
  bool success = base::HexStringToInt(
      command_line.GetSwitchValueASCII(switches::kGpuVendorID),
      reinterpret_cast<int*>(&(gpu_info.gpu.vendor_id)));
  DCHECK(success);
  success = base::HexStringToInt(
      command_line.GetSwitchValueASCII(switches::kGpuDeviceID),
      reinterpret_cast<int*>(&(gpu_info.gpu.device_id)));
  DCHECK(success);
  gpu_info.driver_vendor =
      command_line.GetSwitchValueASCII(switches::kGpuDriverVendor);
  gpu_info.driver_version =
      command_line.GetSwitchValueASCII(switches::kGpuDriverVersion);
  GetContentClient()->SetGpuInfo(gpu_info);

#if defined(OS_WIN)
  // Asynchronously initialize DXVA while GL is being initialized because
  // they both take tens of ms.
  base::WaitableEvent dxva_initialized(true, false);
  DXVAVideoDecodeAccelerator::PreSandboxInitialization(
      base::Bind(&base::WaitableEvent::Signal,
                 base::Unretained(&dxva_initialized)));
#endif

  // We need to track that information for the WarmUpSandbox function.
  bool initialized_gl_context = false;
  // Load and initialize the GL implementation and locate the GL entry points.
  if (gfx::GLSurface::InitializeOneOff()) {
    if (!gpu_info_collector::CollectContextGraphicsInfo(&gpu_info))
      VLOG(1) << "gpu_info_collector::CollectGraphicsInfo failed";
    GetContentClient()->SetGpuInfo(gpu_info);

    // We know that CollectGraphicsInfo will initialize a GLContext.
    initialized_gl_context = true;

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    if (gpu_info.gpu.vendor_id == 0x10de &&  // NVIDIA
        gpu_info.driver_vendor == "NVIDIA") {
      base::ThreadRestrictions::AssertIOAllowed();
      if (access("/dev/nvidiactl", R_OK) != 0) {
        VLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
        dead_on_arrival = true;
      }
    }
#endif  // OS_CHROMEOS
  } else {
    VLOG(1) << "gfx::GLSurface::InitializeOneOff failed";
    dead_on_arrival = true;
  }

  if (enable_watchdog && delayed_watchdog_enable) {
    watchdog_thread = new GpuWatchdogThread(kGpuTimeout);
    watchdog_thread->Start();
  }

  // OSMesa is expected to run very slowly, so disable the watchdog in that
  // case.
  if (enable_watchdog &&
      gfx::GetGLImplementation() == gfx::kGLImplementationOSMesaGL) {
    watchdog_thread->Stop();

    watchdog_thread = NULL;
  }

  {
    const bool should_initialize_gl_context = !initialized_gl_context &&
                                              !dead_on_arrival;
    // Warm up the current process before enabling the sandbox.
    WarmUpSandbox(gpu_info, should_initialize_gl_context);
  }

#if defined(OS_LINUX)
  {
    TRACE_EVENT0("gpu", "Initialize sandbox");
    bool do_init_sandbox = true;

#if defined(OS_CHROMEOS) && defined(NDEBUG)
    // On Chrome OS and when not on a debug build, initialize
    // the GPU process' sandbox only for Intel GPUs.
    do_init_sandbox = gpu_info.gpu.vendor_id == 0x8086;   // Intel GPU.
#endif

    if (do_init_sandbox) {
      if (watchdog_thread.get())
        watchdog_thread->Stop();
      gpu_info.sandboxed = InitializeSandbox();
      if (watchdog_thread.get())
        watchdog_thread->Start();
    }
  }
#endif

#if defined(OS_WIN)
  {
    // DXVA initialization must have completed before the token is lowered.
    TRACE_EVENT0("gpu", "Wait for DXVA initialization");
    dxva_initialized.Wait();
  }

  {
    TRACE_EVENT0("gpu", "Lower token");
    // For windows, if the target_services interface is not zero, the process
    // is sandboxed and we must call LowerToken() before rendering untrusted
    // content.
    sandbox::TargetServices* target_services =
        parameters.sandbox_info->target_services;
    if (target_services) {
      target_services->LowerToken();
      gpu_info.sandboxed = true;
    }
  }
#endif

  GpuProcess gpu_process;

  GpuChildThread* child_thread = new GpuChildThread(watchdog_thread.get(),
                                                    dead_on_arrival, gpu_info);

  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  {
    TRACE_EVENT0("gpu", "Run Message Loop");
    main_message_loop.Run();
  }

  child_thread->StopWatchdog();

  return 0;
}

namespace {

#if defined(OS_LINUX)
void CreateDummyGlContext() {
  scoped_refptr<gfx::GLSurface> surface(
      gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1)));
  if (!surface.get()) {
    VLOG(1) << "gfx::GLSurface::CreateOffscreenGLSurface failed";
    return;
  }

  // On Linux, this is needed to make sure /dev/nvidiactl has
  // been opened and its descriptor cached.
  scoped_refptr<gfx::GLContext> context(
      gfx::GLContext::CreateGLContext(NULL,
                                      surface,
                                      gfx::PreferDiscreteGpu));
  if (!context.get()) {
    VLOG(1) << "gfx::GLContext::CreateGLContext failed";
    return;
  }

  // Similarly, this is needed for /dev/nvidia0.
  if (context->MakeCurrent(surface)) {
    context->ReleaseCurrent(surface.get());
  } else {
    VLOG(1)  << "gfx::GLContext::MakeCurrent failed";
  }
}
#endif

void WarmUpSandbox(const GPUInfo& gpu_info,
                   bool should_initialize_gl_context) {
  {
    TRACE_EVENT0("gpu", "Warm up rand");
    // Warm up the random subsystem, which needs to be done pre-sandbox on all
    // platforms.
    (void) base::RandUint64();
  }
  {
    TRACE_EVENT0("gpu", "Warm up HMAC");
    // Warm up the crypto subsystem, which needs to done pre-sandbox on all
    // platforms.
    crypto::HMAC hmac(crypto::HMAC::SHA256);
    unsigned char key = '\0';
    bool ret = hmac.Init(&key, sizeof(key));
    (void) ret;
  }

#if defined(OS_CHROMEOS) && defined(ARCH_CPU_ARMEL)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseExynosVda))
    ExynosVideoDecodeAccelerator::PreSandboxInitialization();
  else
    OmxVideoDecodeAccelerator::PreSandboxInitialization();
#elif defined(OS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY)
  VaapiVideoDecodeAccelerator::PreSandboxInitialization();
#endif

#if defined(OS_LINUX)
  // We special case Optimus since the vendor_id we see may not be Nvidia.
  bool uses_nvidia_driver = (gpu_info.gpu.vendor_id == 0x10de &&  // NVIDIA.
                             gpu_info.driver_vendor == "NVIDIA") ||
                            gpu_info.optimus;
  if (uses_nvidia_driver && should_initialize_gl_context) {
    // We need this on Nvidia to pre-open /dev/nvidiactl and /dev/nvidia0.
    CreateDummyGlContext();
  }
#endif

#if defined(OS_WIN)
  // Preload these DLL because the sandbox prevents them from loading.
  LoadLibrary(L"setupapi.dll");
#endif
}

}  // namespace.

}  // namespace content
