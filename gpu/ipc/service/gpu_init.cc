// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_init.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_switching.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "gpu/ipc/service/direct_composition_surface_win.h"
#endif

namespace gpu {

namespace {
#if !defined(OS_MACOSX)
void CollectGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);
#if defined(OS_FUCHSIA)
  // TODO(crbug.com/707031): Implement this.
  NOTIMPLEMENTED();
  return;
#else
  TRACE_EVENT0("gpu,startup", "Collect Graphics Info");
  base::TimeTicks before_collect_context_graphics_info = base::TimeTicks::Now();
  CollectInfoResult result = CollectContextGraphicsInfo(gpu_info);
  switch (result) {
    case kCollectInfoFatalFailure:
      LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
      break;
    case kCollectInfoNonFatalFailure:
      DVLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
      break;
    case kCollectInfoNone:
      NOTREACHED();
      break;
    case kCollectInfoSuccess:
      break;
  }

#if defined(OS_WIN)
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLGLES2 &&
      gl::GLSurfaceEGL::IsDirectCompositionSupported() &&
      DirectCompositionSurfaceWin::AreOverlaysSupported()) {
    gpu_info->supports_overlays = true;
  }
#endif  // defined(OS_WIN)

  if (result != kCollectInfoFatalFailure) {
    base::TimeDelta collect_context_time =
        base::TimeTicks::Now() - before_collect_context_graphics_info;
    UMA_HISTOGRAM_TIMES("GPU.CollectContextGraphicsInfo", collect_context_time);
  }
#endif  // defined(OS_FUCHSIA)
}
#endif  // defined(OS_MACOSX)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
bool CanAccessNvidiaDeviceFile() {
  bool res = true;
  base::ThreadRestrictions::AssertIOAllowed();
  if (access("/dev/nvidiactl", R_OK) != 0) {
    DVLOG(1) << "NVIDIA device file /dev/nvidiactl access denied";
    res = false;
  }
  return res;
}
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

}  // namespace

GpuInit::GpuInit() {}

GpuInit::~GpuInit() {
  gpu::StopForceDiscreteGPU();
}

bool GpuInit::InitializeAndStartSandbox(base::CommandLine* command_line,
                                        const GpuPreferences& gpu_preferences) {
  gpu_preferences_ = gpu_preferences;
#if defined(OS_ANDROID)
  // Android doesn't have PCI vendor/device IDs, so collecting GL strings early
  // is necessary.
  CollectGraphicsInfo(&gpu_info_);
  if (gpu_info_.context_info_state == gpu::kCollectInfoFatalFailure)
    return false;
#else
  // Get vendor_id, device_id, driver_version from browser process through
  // commandline switches.
  // TODO(zmo): Collect basic GPU info (without a context) here instead of
  // passing from browser process.
  GetGpuInfoFromCommandLine(*command_line, &gpu_info_);
#endif  // OS_ANDROID
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (gpu_info_.gpu.vendor_id == 0x10de &&  // NVIDIA
      gpu_info_.driver_vendor == "NVIDIA" && !CanAccessNvidiaDeviceFile())
    return false;
#endif
  gpu_info_.in_process_gpu = false;

  // Compute blacklist and driver bug workaround decisions based on basic GPU
  // info.
  gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info_, command_line);
  if (gpu::SwitchableGPUsSupported(gpu_info_, *command_line)) {
    gpu::InitializeSwitchableGPUs(
        gpu_feature_info_.enabled_gpu_driver_bug_workarounds);
  }

  // In addition to disabling the watchdog if the command line switch is
  // present, disable the watchdog on valgrind because the code is expected
  // to run slowly in that case.
  bool enable_watchdog =
      !command_line->HasSwitch(switches::kDisableGpuWatchdog) &&
      !command_line->HasSwitch(switches::kHeadless) && !RunningOnValgrind();

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

  // Start the GPU watchdog only after anything that is expected to be time
  // consuming has completed, otherwise the process is liable to be aborted.
  if (enable_watchdog && !delayed_watchdog_enable) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create();
#if defined(OS_WIN)
    // This is a workaround for an occasional deadlock between watchdog and
    // current thread. Watchdog hangs at thread initialization in
    // __acrt_thread_attach() and current thread in std::setlocale(...)
    // (during InitializeGLOneOff()). Source of the deadlock looks like an old
    // UCRT bug that was supposed to be fixed in 10.0.10586 release of UCRT,
    // but we might have come accross a not-yet-covered scenario.
    // References:
    // https://bugs.python.org/issue26624
    // http://stackoverflow.com/questions/35572792/setlocale-stuck-on-windows
    auto watchdog_started = watchdog_thread_->WaitUntilThreadStarted();
    DCHECK(watchdog_started);
#endif  // OS_WIN
  }

  sandbox_helper_->PreSandboxStartup();

  bool attempted_startsandbox = false;
#if defined(OS_LINUX)
  // On Chrome OS ARM Mali, GPU driver userspace creates threads when
  // initializing a GL context, so start the sandbox early.
  // TODO(zmo): Need to collect OS version before this.
  if (command_line->HasSwitch(switches::kGpuSandboxStartEarly)) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_);
    attempted_startsandbox = true;
  }
#endif  // defined(OS_LINUX)

  base::TimeTicks before_initialize_one_off = base::TimeTicks::Now();

#if defined(USE_OZONE)
  // Initialize Ozone GPU after the watchdog in case it hangs. The sandbox
  // may also have started at this point.
  ui::OzonePlatform::InitParams params;
  params.single_process = false;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  // Load and initialize the GL implementation and locate the GL entry points if
  // needed. This initialization may have already happened if running in the
  // browser process, for example.
  bool gl_initialized = gl::GetGLImplementation() != gl::kGLImplementationNone;
  if (!gl_initialized)
    gl_initialized = gl::init::InitializeGLNoExtensionsOneOff();

  if (!gl_initialized) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return false;
  }

  // We need to collect GL strings (VENDOR, RENDERER) for blacklisting purposes.
  // However, on Mac we don't actually use them. As documented in
  // crbug.com/222934, due to some driver issues, glGetString could take
  // multiple seconds to finish, which in turn cause the GPU process to crash.
  // By skipping the following code on Mac, we don't really lose anything,
  // because the basic GPU information is passed down from the host process.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  CollectGraphicsInfo(&gpu_info_);
  if (gpu_info_.context_info_state == gpu::kCollectInfoFatalFailure)
    return false;
  gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info_, command_line);
#endif

  if (!gpu_feature_info_.disabled_extensions.empty()) {
    gl::init::SetDisabledExtensionsPlatform(
        gpu_feature_info_.disabled_extensions);
  }
  gl_initialized = gl::init::InitializeExtensionSettingsOneOffPlatform();
  if (!gl_initialized) {
    VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
    return false;
  }

  base::TimeDelta initialize_one_off_time =
      base::TimeTicks::Now() - before_initialize_one_off;
  UMA_HISTOGRAM_MEDIUM_TIMES("GPU.InitializeOneOffMediumTime",
                             initialize_one_off_time);

  // Software GL is expected to run slowly, so disable the watchdog
  // in that case.
  if (gl::GetGLImplementation() == gl::GetSoftwareGLImplementation()) {
    if (watchdog_thread_)
      watchdog_thread_->Stop();
    watchdog_thread_ = nullptr;
  } else if (enable_watchdog && delayed_watchdog_enable) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create();
  }

  if (!gpu_info_.sandboxed && !attempted_startsandbox) {
    gpu_info_.sandboxed = sandbox_helper_->EnsureSandboxInitialized(
        watchdog_thread_.get(), &gpu_info_);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.Sandbox.InitializedSuccessfully",
                        gpu_info_.sandboxed);

  gpu_info_.passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(command_line) &&
      gles2::PassthroughCommandDecoderSupported();

  init_successful_ = true;
  return true;
}

void GpuInit::InitializeInProcess(base::CommandLine* command_line,
                                  const GpuPreferences& gpu_preferences,
                                  const GPUInfo* gpu_info,
                                  const GpuFeatureInfo* gpu_feature_info) {
  gpu_preferences_ = gpu_preferences;
  init_successful_ = true;
#if defined(USE_OZONE)
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);
#endif

  if (gpu_info && gpu_feature_info) {
    gpu_info_ = *gpu_info;
    gpu_feature_info_ = *gpu_feature_info;
  } else {
#if defined(OS_ANDROID)
    gpu::CollectContextGraphicsInfo(&gpu_info_);
#else
    // TODO(zmo): Collect basic GPU info here instead.
    gpu::GetGpuInfoFromCommandLine(*command_line, &gpu_info_);
#endif
    gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info_, command_line);
  }

  if (!gl::init::InitializeGLNoExtensionsOneOff()) {
    VLOG(1) << "gl::init::InitializeGLNoExtensionsOneOff failed";
    return;
  }

#if !defined(OS_ANDROID)
  gpu::CollectContextGraphicsInfo(&gpu_info_);
  gpu_feature_info_ = gpu::ComputeGpuFeatureInfo(gpu_info_, command_line);
#endif
  if (!gpu_feature_info_.disabled_extensions.empty()) {
    gl::init::SetDisabledExtensionsPlatform(
        gpu_feature_info_.disabled_extensions);
  }
  if (!gl::init::InitializeExtensionSettingsOneOffPlatform()) {
    VLOG(1) << "gl::init::InitializeExtensionSettingsOneOffPlatform failed";
  }
}

}  // namespace gpu
