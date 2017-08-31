// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_utils.h"

#include "base/command_line.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_features.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

namespace {

bool GetUintFromSwitch(const base::CommandLine* command_line,
                       const base::StringPiece& switch_string,
                       uint32_t* value) {
  if (!command_line->HasSwitch(switch_string))
    return false;
  std::string switch_value(command_line->GetSwitchValueASCII(switch_string));
  return base::StringToUint(switch_value, value);
}

void RunTaskOnTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& callback) {
  task_runner->PostTask(FROM_HERE, callback);
}

void StopGpuProcessImpl(const base::Closure& callback,
                        content::GpuProcessHost* host) {
  if (host)
    host->gpu_service()->Stop(callback);
  else
    callback.Run();
}

}  // namespace

namespace content {

const gpu::GpuPreferences GetGpuPreferencesFromCommandLine() {
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.single_process =
      command_line->HasSwitch(switches::kSingleProcess);
  gpu_preferences.in_process_gpu =
      command_line->HasSwitch(switches::kInProcessGPU);
  gpu_preferences.ui_prioritize_in_gpu_process =
      command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess);
  gpu_preferences.disable_accelerated_video_decode =
      command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode);
#if defined(OS_CHROMEOS)
  gpu_preferences.disable_vaapi_accelerated_video_encode =
      command_line->HasSwitch(switches::kDisableVaapiAcceleratedVideoEncode);
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
  gpu_preferences.disable_web_rtc_hw_encoding =
      command_line->HasSwitch(switches::kDisableWebRtcHWEncoding);
#endif
#if defined(OS_WIN)
  uint32_t enable_accelerated_vpx_decode_val =
      gpu::GpuPreferences::VPX_VENDOR_MICROSOFT;
  if (GetUintFromSwitch(command_line, switches::kEnableAcceleratedVpxDecode,
                        &enable_accelerated_vpx_decode_val)) {
    gpu_preferences.enable_accelerated_vpx_decode =
        static_cast<gpu::GpuPreferences::VpxDecodeVendors>(
            enable_accelerated_vpx_decode_val);
  }
  gpu_preferences.enable_low_latency_dxva =
      !command_line->HasSwitch(switches::kDisableLowLatencyDxva);
  gpu_preferences.enable_zero_copy_dxgi_video =
      !command_line->HasSwitch(switches::kDisableZeroCopyDxgiVideo);
  gpu_preferences.enable_nv12_dxgi_video =
      !command_line->HasSwitch(switches::kDisableNv12DxgiVideo);
#endif
  gpu_preferences.compile_shader_always_succeeds =
      command_line->HasSwitch(switches::kCompileShaderAlwaysSucceeds);
  gpu_preferences.disable_gl_error_limit =
      command_line->HasSwitch(switches::kDisableGLErrorLimit);
  gpu_preferences.disable_glsl_translator =
      command_line->HasSwitch(switches::kDisableGLSLTranslator);
  gpu_preferences.disable_gpu_driver_bug_workarounds =
      command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds);
  gpu_preferences.disable_shader_name_hashing =
      command_line->HasSwitch(switches::kDisableShaderNameHashing);
  gpu_preferences.enable_gpu_command_logging =
      command_line->HasSwitch(switches::kEnableGPUCommandLogging);
  gpu_preferences.enable_gpu_debugging =
      command_line->HasSwitch(switches::kEnableGPUDebugging);
  gpu_preferences.enable_gpu_service_logging_gpu =
      command_line->HasSwitch(switches::kEnableGPUServiceLoggingGPU);
  gpu_preferences.enable_gpu_driver_debug_logging =
      command_line->HasSwitch(switches::kEnableGPUDriverDebugLogging);
  gpu_preferences.disable_gpu_program_cache =
      command_line->HasSwitch(switches::kDisableGpuProgramCache);
  gpu_preferences.enforce_gl_minimums =
      command_line->HasSwitch(switches::kEnforceGLMinimums);
  if (GetUintFromSwitch(command_line, switches::kForceGpuMemAvailableMb,
                        &gpu_preferences.force_gpu_mem_available)) {
    gpu_preferences.force_gpu_mem_available *= 1024 * 1024;
  }
  if (GetUintFromSwitch(command_line, switches::kGpuProgramCacheSizeKb,
                        &gpu_preferences.gpu_program_cache_size)) {
    gpu_preferences.gpu_program_cache_size *= 1024;
  }
  gpu_preferences.disable_gpu_shader_disk_cache =
      command_line->HasSwitch(switches::kDisableGpuShaderDiskCache);
  gpu_preferences.enable_threaded_texture_mailboxes =
      command_line->HasSwitch(switches::kEnableThreadedTextureMailboxes);
  gpu_preferences.gl_shader_interm_output =
      command_line->HasSwitch(switches::kGLShaderIntermOutput);
  gpu_preferences.emulate_shader_precision =
      command_line->HasSwitch(switches::kEmulateShaderPrecision);
  gpu_preferences.enable_gpu_service_logging =
      command_line->HasSwitch(switches::kEnableGPUServiceLogging);
  gpu_preferences.enable_gpu_service_tracing =
      command_line->HasSwitch(switches::kEnableGPUServiceTracing);
  gpu_preferences.use_passthrough_cmd_decoder =
      gl::UsePassthroughCommandDecoder(command_line);
  // Some of these preferences are set or adjusted in
  // GpuDataManagerImplPrivate::AppendGpuCommandLine.
  return gpu_preferences;
}

void StopGpuProcess(const base::Closure& callback) {
  content::GpuProcessHost::CallOnIO(
      content::GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
      false /* force_create */,
      base::Bind(&StopGpuProcessImpl,
                 base::Bind(RunTaskOnTaskRunner,
                            base::ThreadTaskRunnerHandle::Get(), callback)));
}

}  // namespace content
