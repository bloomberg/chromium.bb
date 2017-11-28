// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PREFERENCES_STRUCT_TRAITS_H_
#define GPU_IPC_COMMON_GPU_PREFERENCES_STRUCT_TRAITS_H_

#include "gpu/ipc/common/gpu_preferences.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gpu::mojom::VpxDecodeVendors,
                  gpu::GpuPreferences::VpxDecodeVendors> {
  static gpu::mojom::VpxDecodeVendors ToMojom(
      gpu::GpuPreferences::VpxDecodeVendors vpx) {
    switch (vpx) {
      case gpu::GpuPreferences::VPX_VENDOR_NONE:
        return gpu::mojom::VpxDecodeVendors::VPX_VENDOR_NONE;
      case gpu::GpuPreferences::VPX_VENDOR_MICROSOFT:
        return gpu::mojom::VpxDecodeVendors::VPX_VENDOR_MICROSOFT;
      case gpu::GpuPreferences::VPX_VENDOR_AMD:
        return gpu::mojom::VpxDecodeVendors::VPX_VENDOR_AMD;
      case gpu::GpuPreferences::VPX_VENDOR_ALL:
        return gpu::mojom::VpxDecodeVendors::VPX_VENDOR_ALL;
    }
    NOTREACHED();
    return gpu::mojom::VpxDecodeVendors::VPX_VENDOR_NONE;
  }

  static bool FromMojom(gpu::mojom::VpxDecodeVendors input,
                        gpu::GpuPreferences::VpxDecodeVendors* out) {
    switch (input) {
      case gpu::mojom::VpxDecodeVendors::VPX_VENDOR_NONE:
        *out = gpu::GpuPreferences::VPX_VENDOR_NONE;
        return true;
      case gpu::mojom::VpxDecodeVendors::VPX_VENDOR_MICROSOFT:
        *out = gpu::GpuPreferences::VPX_VENDOR_MICROSOFT;
        return true;
      case gpu::mojom::VpxDecodeVendors::VPX_VENDOR_AMD:
        *out = gpu::GpuPreferences::VPX_VENDOR_AMD;
        return true;
      case gpu::mojom::VpxDecodeVendors::VPX_VENDOR_ALL:
        *out = gpu::GpuPreferences::VPX_VENDOR_ALL;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<gpu::mojom::GpuPreferencesDataView, gpu::GpuPreferences> {
  static bool Read(gpu::mojom::GpuPreferencesDataView prefs,
                   gpu::GpuPreferences* out) {
    out->single_process = prefs.single_process();
    out->in_process_gpu = prefs.in_process_gpu();
    out->disable_accelerated_video_decode =
        prefs.disable_accelerated_video_decode();
    out->gpu_startup_dialog = prefs.gpu_startup_dialog();
    out->disable_gpu_watchdog = prefs.disable_gpu_watchdog();
    out->gpu_sandbox_start_early = prefs.gpu_sandbox_start_early();
    out->disable_vaapi_accelerated_video_encode =
        prefs.disable_vaapi_accelerated_video_encode();
    out->disable_web_rtc_hw_encoding = prefs.disable_web_rtc_hw_encoding();
    if (!prefs.ReadEnableAcceleratedVpxDecode(
            &out->enable_accelerated_vpx_decode))
      return false;
    out->enable_low_latency_dxva = prefs.enable_low_latency_dxva();
    out->enable_zero_copy_dxgi_video = prefs.enable_zero_copy_dxgi_video();
    out->enable_nv12_dxgi_video = prefs.enable_nv12_dxgi_video();
    out->enable_media_foundation_vea_on_windows7 =
        prefs.enable_media_foundation_vea_on_windows7();
    out->compile_shader_always_succeeds =
        prefs.compile_shader_always_succeeds();
    out->disable_gl_error_limit = prefs.disable_gl_error_limit();
    out->disable_glsl_translator = prefs.disable_glsl_translator();
    out->disable_gpu_driver_bug_workarounds =
        prefs.disable_gpu_driver_bug_workarounds();
    out->disable_shader_name_hashing = prefs.disable_shader_name_hashing();
    out->enable_gpu_command_logging = prefs.enable_gpu_command_logging();
    out->enable_gpu_debugging = prefs.enable_gpu_debugging();
    out->enable_gpu_service_logging_gpu =
        prefs.enable_gpu_service_logging_gpu();
    out->enable_gpu_driver_debug_logging =
        prefs.enable_gpu_driver_debug_logging();
    out->disable_gpu_program_cache = prefs.disable_gpu_program_cache();
    out->enforce_gl_minimums = prefs.enforce_gl_minimums();
    out->force_gpu_mem_available = prefs.force_gpu_mem_available();
    out->gpu_program_cache_size = prefs.gpu_program_cache_size();
    out->disable_gpu_shader_disk_cache = prefs.disable_gpu_shader_disk_cache();
    out->enable_threaded_texture_mailboxes =
        prefs.enable_threaded_texture_mailboxes();
    out->gl_shader_interm_output = prefs.gl_shader_interm_output();
    out->emulate_shader_precision = prefs.emulate_shader_precision();
    out->enable_raster_decoder = prefs.enable_raster_decoder();
    out->enable_gpu_service_logging = prefs.enable_gpu_service_logging();
    out->enable_gpu_service_tracing = prefs.enable_gpu_service_tracing();
    out->use_passthrough_cmd_decoder = prefs.use_passthrough_cmd_decoder();
    return true;
  }

  static bool single_process(const gpu::GpuPreferences& prefs) {
    return prefs.single_process;
  }
  static bool in_process_gpu(const gpu::GpuPreferences& prefs) {
    return prefs.in_process_gpu;
  }
  static bool disable_accelerated_video_decode(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_accelerated_video_decode;
  }
  static bool gpu_startup_dialog(const gpu::GpuPreferences& prefs) {
    return prefs.gpu_startup_dialog;
  }
  static bool disable_gpu_watchdog(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_watchdog;
  }
  static bool gpu_sandbox_start_early(const gpu::GpuPreferences& prefs) {
    return prefs.gpu_sandbox_start_early;
  }

  static bool disable_vaapi_accelerated_video_encode(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_vaapi_accelerated_video_encode;
  }

  static bool disable_web_rtc_hw_encoding(const gpu::GpuPreferences& prefs) {
    return prefs.disable_web_rtc_hw_encoding;
  }

  static gpu::GpuPreferences::VpxDecodeVendors enable_accelerated_vpx_decode(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_accelerated_vpx_decode;
  }
  static bool enable_low_latency_dxva(const gpu::GpuPreferences& prefs) {
    return prefs.enable_low_latency_dxva;
  }
  static bool enable_zero_copy_dxgi_video(const gpu::GpuPreferences& prefs) {
    return prefs.enable_zero_copy_dxgi_video;
  }
  static bool enable_nv12_dxgi_video(const gpu::GpuPreferences& prefs) {
    return prefs.enable_nv12_dxgi_video;
  }
  static bool enable_media_foundation_vea_on_windows7(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_media_foundation_vea_on_windows7;
  }
  static bool compile_shader_always_succeeds(const gpu::GpuPreferences& prefs) {
    return prefs.compile_shader_always_succeeds;
  }
  static bool disable_gl_error_limit(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gl_error_limit;
  }
  static bool disable_glsl_translator(const gpu::GpuPreferences& prefs) {
    return prefs.disable_glsl_translator;
  }
  static bool disable_gpu_driver_bug_workarounds(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_driver_bug_workarounds;
  }
  static bool disable_shader_name_hashing(const gpu::GpuPreferences& prefs) {
    return prefs.disable_shader_name_hashing;
  }
  static bool enable_gpu_command_logging(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_command_logging;
  }
  static bool enable_gpu_debugging(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_debugging;
  }
  static bool enable_gpu_service_logging_gpu(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_service_logging_gpu;
  }
  static bool enable_gpu_driver_debug_logging(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_driver_debug_logging;
  }
  static bool disable_gpu_program_cache(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_program_cache;
  }
  static bool enforce_gl_minimums(const gpu::GpuPreferences& prefs) {
    return prefs.enforce_gl_minimums;
  }
  static uint32_t force_gpu_mem_available(const gpu::GpuPreferences& prefs) {
    return prefs.force_gpu_mem_available;
  }
  static uint32_t gpu_program_cache_size(const gpu::GpuPreferences& prefs) {
    return prefs.gpu_program_cache_size;
  }
  static bool disable_gpu_shader_disk_cache(const gpu::GpuPreferences& prefs) {
    return prefs.disable_gpu_shader_disk_cache;
  }
  static bool enable_threaded_texture_mailboxes(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_threaded_texture_mailboxes;
  }
  static bool gl_shader_interm_output(const gpu::GpuPreferences& prefs) {
    return prefs.gl_shader_interm_output;
  }
  static bool emulate_shader_precision(const gpu::GpuPreferences& prefs) {
    return prefs.emulate_shader_precision;
  }
  static bool enable_raster_decoder(const gpu::GpuPreferences& prefs) {
    return prefs.enable_raster_decoder;
  }
  static bool enable_gpu_service_logging(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_service_logging;
  }
  static bool enable_gpu_service_tracing(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_service_tracing;
  }
  static bool use_passthrough_cmd_decoder(const gpu::GpuPreferences& prefs) {
    return prefs.use_passthrough_cmd_decoder;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_PREFERENCES_STRUCT_TRAITS_H_
