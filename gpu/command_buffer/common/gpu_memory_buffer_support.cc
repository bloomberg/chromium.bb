// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2extchromium.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"

namespace gpu {

unsigned InternalFormatForGpuMemoryBufferFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED_EXT;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG_EXT;
    case gfx::BufferFormat::BGR_565:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_4444:
      return GL_RGBA;
    case gfx::BufferFormat::RGBX_8888:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_8888:
      return GL_RGBA;
    case gfx::BufferFormat::BGRX_8888:
      return GL_RGB;
    case gfx::BufferFormat::BGRX_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::RGBX_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::YVU_420:
      return GL_RGB_YCRCB_420_CHROMIUM;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return GL_RGB_YCBCR_420V_CHROMIUM;
    case gfx::BufferFormat::UYVY_422:
      return GL_RGB_YCBCR_422_CHROMIUM;
    default:
      NOTREACHED();
      return 0;
  }
}

bool IsImageFromGpuMemoryBufferFormatSupported(
    gfx::BufferFormat format,
    const gpu::Capabilities& capabilities) {
  return capabilities.gpu_memory_buffer_formats.Has(format);
}

bool IsImageSizeValidForGpuMemoryBufferFormat(const gfx::Size& size,
                                              gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      // U and V planes are subsampled by a factor of 2.
      return size.width() % 2 == 0 && size.height() % 2 == 0;
    case gfx::BufferFormat::UYVY_422:
      return size.width() % 2 == 0;
  }

  NOTREACHED();
  return false;
}

uint32_t GetPlatformSpecificTextureTarget() {
#if defined(OS_MACOSX)
  return GL_TEXTURE_RECTANGLE_ARB;
#elif defined(OS_ANDROID) || defined(OS_LINUX)
  return GL_TEXTURE_EXTERNAL_OES;
#elif defined(OS_WIN) || defined(OS_FUCHSIA)
  return GL_TEXTURE_2D;
#elif defined(OS_NACL)
  NOTREACHED();
  return 0;
#else
#error Unsupported OS
#endif
}

GPU_EXPORT uint32_t GetBufferTextureTarget(gfx::BufferUsage usage,
                                           gfx::BufferFormat format,
                                           const Capabilities& capabilities) {
  bool found = base::ContainsValue(capabilities.texture_target_exception_list,
                                   gfx::BufferUsageAndFormat(usage, format));
  return found ? gpu::GetPlatformSpecificTextureTarget() : GL_TEXTURE_2D;
}

GPU_EXPORT bool NativeBufferNeedsPlatformSpecificTextureTarget(
    gfx::BufferFormat format) {
#if defined(USE_OZONE)
  // Always use GL_TEXTURE_2D as the target for RGB textures.
  // https://crbug.com/916728
  if (format == gfx::BufferFormat::R_8 || format == gfx::BufferFormat::RG_88 ||
      format == gfx::BufferFormat::RGBA_8888 ||
      format == gfx::BufferFormat::BGRA_8888 ||
      format == gfx::BufferFormat::RGBX_8888 ||
      format == gfx::BufferFormat::BGRX_8888) {
    return false;
  }
#elif defined(OS_ANDROID)
  if (format == gfx::BufferFormat::BGR_565 ||
      format == gfx::BufferFormat::RGBA_8888) {
    return false;
  }
#endif
  return true;
}

}  // namespace gpu
