// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_factory.h"

#include "ui/gl/gl_bindings.h"

namespace gpu {

ImageFactory::ImageFactory() {
}

ImageFactory::~ImageFactory() {
}

// static
gfx::GpuMemoryBuffer::Format ImageFactory::ImageFormatToGpuMemoryBufferFormat(
    unsigned internalformat) {
  switch (internalformat) {
    case GL_RGB:
      return gfx::GpuMemoryBuffer::RGBX_8888;
    case GL_RGBA:
      return gfx::GpuMemoryBuffer::RGBA_8888;
    default:
      NOTREACHED();
      return gfx::GpuMemoryBuffer::RGBA_8888;
  }
}

// static
gfx::GpuMemoryBuffer::Usage ImageFactory::ImageUsageToGpuMemoryBufferUsage(
    unsigned usage) {
  switch (usage) {
    case GL_MAP_CHROMIUM:
      return gfx::GpuMemoryBuffer::MAP;
    case GL_SCANOUT_CHROMIUM:
      return gfx::GpuMemoryBuffer::SCANOUT;
    default:
      NOTREACHED();
      return gfx::GpuMemoryBuffer::MAP;
  }
}

// static
bool ImageFactory::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
    unsigned internalformat,
    gfx::GpuMemoryBuffer::Format format) {
  switch (internalformat) {
    case GL_RGB:
      switch (format) {
        case gfx::GpuMemoryBuffer::RGBX_8888:
          return true;
        case gfx::GpuMemoryBuffer::RGBA_8888:
        case gfx::GpuMemoryBuffer::BGRA_8888:
          return false;
      }
      NOTREACHED();
      return false;
    case GL_RGBA:
      switch (format) {
        case gfx::GpuMemoryBuffer::RGBX_8888:
          return false;
        case gfx::GpuMemoryBuffer::RGBA_8888:
        case gfx::GpuMemoryBuffer::BGRA_8888:
          return true;
      }
      NOTREACHED();
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace gpu
