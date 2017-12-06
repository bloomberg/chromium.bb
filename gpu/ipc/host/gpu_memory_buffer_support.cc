// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/gpu_memory_buffer_support.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/gpu_switches.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

bool AreNativeGpuMemoryBuffersEnabled() {
  // Disable native buffers when using software GL.
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseGL) ==
      gl::GetGLImplementationName(gl::GetSoftwareGLImplementation())) {
    return false;
  }

#if defined(OS_MACOSX)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNativeGpuMemoryBuffers);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNativeGpuMemoryBuffers);
#endif
}

GpuMemoryBufferConfigurationSet GetNativeGpuMemoryBufferConfigurations() {
  GpuMemoryBufferConfigurationSet configurations;

#if defined(USE_OZONE) || defined(OS_MACOSX) || defined(OS_WIN) || \
    defined(OS_ANDROID)
  if (AreNativeGpuMemoryBuffersEnabled()) {
    const gfx::BufferFormat kNativeFormats[] = {
        gfx::BufferFormat::R_8,       gfx::BufferFormat::RG_88,
        gfx::BufferFormat::R_16,      gfx::BufferFormat::BGR_565,
        gfx::BufferFormat::RGBA_4444, gfx::BufferFormat::RGBA_8888,
        gfx::BufferFormat::BGRA_8888, gfx::BufferFormat::BGRX_1010102,
        gfx::BufferFormat::RGBA_F16,  gfx::BufferFormat::UYVY_422,
        gfx::BufferFormat::YVU_420,   gfx::BufferFormat::YUV_420_BIPLANAR};
    const gfx::BufferUsage kNativeUsages[] = {
        gfx::BufferUsage::GPU_READ, gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT};
    for (auto format : kNativeFormats) {
      for (auto usage : kNativeUsages) {
        if (IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
          configurations.insert(std::make_pair(format, usage));
      }
    }
  }

  // Disable native buffers only when using software GL.
  bool force_native_gpu_read_write_formats =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseGL) !=
      gl::GetGLImplementationName(gl::GetSoftwareGLImplementation());
  if (force_native_gpu_read_write_formats) {
    const gfx::BufferFormat kGPUReadWriteFormats[] = {
        gfx::BufferFormat::BGR_565,   gfx::BufferFormat::RGBA_8888,
        gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::BGRA_8888,
        gfx::BufferFormat::BGRX_8888, gfx::BufferFormat::UYVY_422,
        gfx::BufferFormat::YVU_420,   gfx::BufferFormat::YUV_420_BIPLANAR};
    const gfx::BufferUsage kGPUReadWriteUsages[] = {
        gfx::BufferUsage::GPU_READ, gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VDA_WRITE};
    for (auto format : kGPUReadWriteFormats) {
      for (auto usage : kGPUReadWriteUsages) {
        if (IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
          configurations.insert(std::make_pair(format, usage));
      }
    }
  }
#endif  // defined(USE_OZONE) || defined(OS_MACOSX) || defined(OS_WIN)

  return configurations;
}

bool GetImageNeedsPlatformSpecificTextureTarget(gfx::BufferFormat format,
                                                gfx::BufferUsage usage) {
#if defined(USE_OZONE) || defined(OS_MACOSX) || defined(OS_WIN) || \
    defined(OS_ANDROID)
  GpuMemoryBufferConfigurationSet native_configurations =
      GetNativeGpuMemoryBufferConfigurations();
  return native_configurations.find(std::make_pair(format, usage)) !=
         native_configurations.end();
#else  // defined(USE_OZONE) || defined(OS_MACOSX)
  return false;
#endif
}

}  // namespace gpu
