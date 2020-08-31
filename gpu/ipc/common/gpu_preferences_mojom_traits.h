// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PREFERENCES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_GPU_PREFERENCES_MOJOM_TRAITS_H_

#include <vector>

#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_preferences.mojom-shared.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

#if defined(USE_OZONE)
#include "base/message_loop/message_pump_type.h"
#include "mojo/public/cpp/base/message_pump_type_mojom_traits.h"
#endif

namespace mojo {

template <>
struct EnumTraits<gpu::mojom::GrContextType, gpu::GrContextType> {
  static gpu::mojom::GrContextType ToMojom(gpu::GrContextType input) {
    switch (input) {
      case gpu::GrContextType::kGL:
        return gpu::mojom::GrContextType::kGL;
      case gpu::GrContextType::kVulkan:
        return gpu::mojom::GrContextType::kVulkan;
      case gpu::GrContextType::kMetal:
        return gpu::mojom::GrContextType::kMetal;
      case gpu::GrContextType::kDawn:
        return gpu::mojom::GrContextType::kDawn;
    }
    NOTREACHED();
    return gpu::mojom::GrContextType::kGL;
  }
  static bool FromMojom(gpu::mojom::GrContextType input,
                        gpu::GrContextType* out) {
    switch (input) {
      case gpu::mojom::GrContextType::kGL:
        *out = gpu::GrContextType::kGL;
        return true;
      case gpu::mojom::GrContextType::kVulkan:
        *out = gpu::GrContextType::kVulkan;
        return true;
      case gpu::mojom::GrContextType::kMetal:
        *out = gpu::GrContextType::kMetal;
        return true;
      case gpu::mojom::GrContextType::kDawn:
        *out = gpu::GrContextType::kDawn;
        return true;
    }
    return false;
  }
};

template <>
struct EnumTraits<gpu::mojom::VulkanImplementationName,
                  gpu::VulkanImplementationName> {
  static gpu::mojom::VulkanImplementationName ToMojom(
      gpu::VulkanImplementationName input) {
    switch (input) {
      case gpu::VulkanImplementationName::kNone:
        return gpu::mojom::VulkanImplementationName::kNone;
      case gpu::VulkanImplementationName::kNative:
        return gpu::mojom::VulkanImplementationName::kNative;
      case gpu::VulkanImplementationName::kForcedNative:
        return gpu::mojom::VulkanImplementationName::kForcedNative;
      case gpu::VulkanImplementationName::kSwiftshader:
        return gpu::mojom::VulkanImplementationName::kSwiftshader;
    }
    NOTREACHED();
    return gpu::mojom::VulkanImplementationName::kNone;
  }
  static bool FromMojom(gpu::mojom::VulkanImplementationName input,
                        gpu::VulkanImplementationName* out) {
    switch (input) {
      case gpu::mojom::VulkanImplementationName::kNone:
        *out = gpu::VulkanImplementationName::kNone;
        return true;
      case gpu::mojom::VulkanImplementationName::kNative:
        *out = gpu::VulkanImplementationName::kNative;
        return true;
      case gpu::mojom::VulkanImplementationName::kForcedNative:
        *out = gpu::VulkanImplementationName::kForcedNative;
        return true;
      case gpu::mojom::VulkanImplementationName::kSwiftshader:
        *out = gpu::VulkanImplementationName::kSwiftshader;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<gpu::mojom::GpuPreferencesDataView, gpu::GpuPreferences> {
  static bool Read(gpu::mojom::GpuPreferencesDataView prefs,
                   gpu::GpuPreferences* out) {
    out->disable_accelerated_video_decode =
        prefs.disable_accelerated_video_decode();
    out->disable_accelerated_video_encode =
        prefs.disable_accelerated_video_encode();
    out->gpu_startup_dialog = prefs.gpu_startup_dialog();
    out->disable_gpu_watchdog = prefs.disable_gpu_watchdog();
    out->gpu_sandbox_start_early = prefs.gpu_sandbox_start_early();
    out->enable_low_latency_dxva = prefs.enable_low_latency_dxva();
    out->enable_zero_copy_dxgi_video = prefs.enable_zero_copy_dxgi_video();
    out->enable_nv12_dxgi_video = prefs.enable_nv12_dxgi_video();
    out->enable_media_foundation_vea_on_windows7 =
        prefs.enable_media_foundation_vea_on_windows7();
    out->disable_software_rasterizer = prefs.disable_software_rasterizer();
    out->log_gpu_control_list_decisions =
        prefs.log_gpu_control_list_decisions();
    out->compile_shader_always_succeeds =
        prefs.compile_shader_always_succeeds();
    out->disable_gl_error_limit = prefs.disable_gl_error_limit();
    out->disable_glsl_translator = prefs.disable_glsl_translator();
    out->disable_shader_name_hashing = prefs.disable_shader_name_hashing();
    out->enable_gpu_command_logging = prefs.enable_gpu_command_logging();
    out->enable_gpu_debugging = prefs.enable_gpu_debugging();
    out->enable_gpu_service_logging_gpu =
        prefs.enable_gpu_service_logging_gpu();
    out->enable_gpu_driver_debug_logging =
        prefs.enable_gpu_driver_debug_logging();
    out->disable_gpu_program_cache = prefs.disable_gpu_program_cache();
    out->enforce_gl_minimums = prefs.enforce_gl_minimums();
    out->force_gpu_mem_available_bytes = prefs.force_gpu_mem_available_bytes();
    out->force_gpu_mem_discardable_limit_bytes =
        prefs.force_gpu_mem_discardable_limit_bytes();
    out->force_max_texture_size = prefs.force_max_texture_size();
    out->gpu_program_cache_size = prefs.gpu_program_cache_size();
    out->disable_gpu_shader_disk_cache = prefs.disable_gpu_shader_disk_cache();
    out->enable_threaded_texture_mailboxes =
        prefs.enable_threaded_texture_mailboxes();
    out->gl_shader_interm_output = prefs.gl_shader_interm_output();
    out->emulate_shader_precision = prefs.emulate_shader_precision();
    out->enable_android_surface_control =
        prefs.enable_android_surface_control();
    out->enable_gpu_service_logging = prefs.enable_gpu_service_logging();
    out->enable_gpu_service_tracing = prefs.enable_gpu_service_tracing();
    out->use_passthrough_cmd_decoder = prefs.use_passthrough_cmd_decoder();
    out->disable_biplanar_gpu_memory_buffers_for_video_frames =
        prefs.disable_biplanar_gpu_memory_buffers_for_video_frames();

    mojo::ArrayDataView<gfx::mojom::BufferUsageAndFormatDataView>
        usage_and_format_list;
    prefs.GetTextureTargetExceptionListDataView(&usage_and_format_list);
    for (size_t i = 0; i < usage_and_format_list.size(); ++i) {
      gfx::BufferUsageAndFormat usage_format;
      if (!usage_and_format_list.Read(i, &usage_format))
        return false;
      out->texture_target_exception_list.push_back(usage_format);
    }

    out->ignore_gpu_blacklist = prefs.ignore_gpu_blacklist();
    out->enable_oop_rasterization = prefs.enable_oop_rasterization();
    out->disable_oop_rasterization = prefs.disable_oop_rasterization();
    out->enable_oop_rasterization_ddl = prefs.enable_oop_rasterization_ddl();
    out->watchdog_starts_backgrounded = prefs.watchdog_starts_backgrounded();
    if (!prefs.ReadGrContextType(&out->gr_context_type))
      return false;
    if (!prefs.ReadUseVulkan(&out->use_vulkan))
      return false;
    out->enforce_vulkan_protected_memory =
        prefs.enforce_vulkan_protected_memory();
    out->disable_vulkan_surface = prefs.disable_vulkan_surface();
    out->disable_vulkan_fallback_to_gl_for_testing =
        prefs.disable_vulkan_fallback_to_gl_for_testing();
    out->enable_metal = prefs.enable_metal();
    out->enable_gpu_benchmarking_extension =
        prefs.enable_gpu_benchmarking_extension();
    out->enable_webgpu = prefs.enable_webgpu();
    out->enable_dawn_backend_validation =
        prefs.enable_dawn_backend_validation();
    out->enable_gpu_blocked_time_metric =
        prefs.enable_gpu_blocked_time_metric();
    out->enable_perf_data_collection = prefs.enable_perf_data_collection();

#if defined(USE_OZONE)
    if (!prefs.ReadMessagePumpType(&out->message_pump_type))
      return false;
#endif

    out->enable_native_gpu_memory_buffers =
        prefs.enable_native_gpu_memory_buffers();

    out->force_disable_new_accelerated_video_decoder =
        prefs.force_disable_new_accelerated_video_decoder();

    return true;
  }

  static bool disable_accelerated_video_decode(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_accelerated_video_decode;
  }
  static bool disable_accelerated_video_encode(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_accelerated_video_encode;
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
  static bool disable_software_rasterizer(const gpu::GpuPreferences& prefs) {
    return prefs.disable_software_rasterizer;
  }
  static bool log_gpu_control_list_decisions(const gpu::GpuPreferences& prefs) {
    return prefs.log_gpu_control_list_decisions;
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
  static uint32_t force_gpu_mem_available_bytes(
      const gpu::GpuPreferences& prefs) {
    return prefs.force_gpu_mem_available_bytes;
  }
  static uint32_t force_gpu_mem_discardable_limit_bytes(
      const gpu::GpuPreferences& prefs) {
    return prefs.force_gpu_mem_discardable_limit_bytes;
  }
  static uint32_t force_max_texture_size(const gpu::GpuPreferences& prefs) {
    return prefs.force_max_texture_size;
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
  static bool enable_android_surface_control(const gpu::GpuPreferences& prefs) {
    return prefs.enable_android_surface_control;
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
  static bool disable_biplanar_gpu_memory_buffers_for_video_frames(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_biplanar_gpu_memory_buffers_for_video_frames;
  }
  static const std::vector<gfx::BufferUsageAndFormat>&
  texture_target_exception_list(const gpu::GpuPreferences& prefs) {
    return prefs.texture_target_exception_list;
  }
  static bool ignore_gpu_blacklist(const gpu::GpuPreferences& prefs) {
    return prefs.ignore_gpu_blacklist;
  }
  static bool enable_oop_rasterization(const gpu::GpuPreferences& prefs) {
    return prefs.enable_oop_rasterization;
  }
  static bool disable_oop_rasterization(const gpu::GpuPreferences& prefs) {
    return prefs.disable_oop_rasterization;
  }
  static bool enable_oop_rasterization_ddl(const gpu::GpuPreferences& prefs) {
    return prefs.enable_oop_rasterization_ddl;
  }
  static bool watchdog_starts_backgrounded(const gpu::GpuPreferences& prefs) {
    return prefs.watchdog_starts_backgrounded;
  }
  static gpu::GrContextType gr_context_type(const gpu::GpuPreferences& prefs) {
    return prefs.gr_context_type;
  }
  static gpu::VulkanImplementationName use_vulkan(
      const gpu::GpuPreferences& prefs) {
    return prefs.use_vulkan;
  }
  static bool enforce_vulkan_protected_memory(
      const gpu::GpuPreferences& prefs) {
    return prefs.enforce_vulkan_protected_memory;
  }
  static bool disable_vulkan_surface(const gpu::GpuPreferences& prefs) {
    return prefs.disable_vulkan_surface;
  }
  static bool disable_vulkan_fallback_to_gl_for_testing(
      const gpu::GpuPreferences& prefs) {
    return prefs.disable_vulkan_fallback_to_gl_for_testing;
  }
  static bool enable_metal(const gpu::GpuPreferences& prefs) {
    return prefs.enable_metal;
  }
  static bool enable_gpu_benchmarking_extension(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_benchmarking_extension;
  }
  static bool enable_webgpu(const gpu::GpuPreferences& prefs) {
    return prefs.enable_webgpu;
  }
  static bool enable_dawn_backend_validation(const gpu::GpuPreferences& prefs) {
    return prefs.enable_dawn_backend_validation;
  }
  static bool enable_gpu_blocked_time_metric(const gpu::GpuPreferences& prefs) {
    return prefs.enable_gpu_blocked_time_metric;
  }
  static bool enable_perf_data_collection(const gpu::GpuPreferences& prefs) {
    return prefs.enable_perf_data_collection;
  }
#if defined(USE_OZONE)
  static base::MessagePumpType message_pump_type(
      const gpu::GpuPreferences& prefs) {
    return prefs.message_pump_type;
  }
#endif
  static bool enable_native_gpu_memory_buffers(
      const gpu::GpuPreferences& prefs) {
    return prefs.enable_native_gpu_memory_buffers;
  }
  static bool force_disable_new_accelerated_video_decoder(
      const gpu::GpuPreferences& prefs) {
    return prefs.force_disable_new_accelerated_video_decoder;
  }
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_GPU_PREFERENCES_MOJOM_TRAITS_H_
