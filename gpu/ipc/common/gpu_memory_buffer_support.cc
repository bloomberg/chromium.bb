// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_memory_buffer_support.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"

#if defined(OS_LINUX)
#include "ui/gfx/client_native_pixmap_factory.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

namespace gpu {

gfx::GpuMemoryBufferType GetNativeGpuMemoryBufferType() {
#if defined(OS_MACOSX)
  return gfx::IO_SURFACE_BUFFER;
#elif defined(OS_ANDROID)
  return gfx::ANDROID_HARDWARE_BUFFER;
#elif defined(OS_LINUX)
  return gfx::NATIVE_PIXMAP;
#elif defined(OS_WIN)
  return gfx::DXGI_SHARED_HANDLE;
#else
  return gfx::EMPTY_BUFFER;
#endif
}

bool IsNativeGpuMemoryBufferConfigurationSupported(gfx::BufferFormat format,
                                                   gfx::BufferUsage usage) {
  DCHECK_NE(gfx::SHARED_MEMORY_BUFFER, GetNativeGpuMemoryBufferType());

#if defined(OS_MACOSX)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::BGRX_8888 ||
             format == gfx::BufferFormat::R_8 ||
             format == gfx::BufferFormat::RGBA_F16 ||
             format == gfx::BufferFormat::UYVY_422 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::BGRX_8888;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      return format == gfx::BufferFormat::R_8 ||
             format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::RGBA_F16 ||
             format == gfx::BufferFormat::UYVY_422 ||
             format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return false;
  }
  NOTREACHED();
  return false;
#elif defined(OS_ANDROID)
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable()) {
    return false;
  }
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return false;
  }
  NOTREACHED();
  return false;
#elif defined(OS_LINUX)
  if (!gfx::ClientNativePixmapFactory::GetInstance()) {
    // unittests don't have to set ClientNativePixmapFactory.
    return false;
  }
  return gfx::ClientNativePixmapFactory::GetInstance()
      ->IsConfigurationSupported(format, usage);
#elif defined(OS_WIN)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      return false;
    default:
      NOTREACHED();
      return false;
  }
#else
  DCHECK_EQ(GetNativeGpuMemoryBufferType(), gfx::EMPTY_BUFFER);
  return false;
#endif
}

}  // namespace gpu
