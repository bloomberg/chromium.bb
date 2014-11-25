// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory.h"

#include "base/logging.h"
#include "content/common/gpu/gpu_memory_buffer_factory_shared_memory.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"
#endif

namespace content {

// static
void GpuMemoryBufferFactory::GetSupportedTypes(
    std::vector<gfx::GpuMemoryBufferType>* types) {
  const gfx::GpuMemoryBufferType supported_types[] = {
#if defined(OS_MACOSX)
    gfx::IO_SURFACE_BUFFER,
#endif
#if defined(OS_ANDROID)
    gfx::SURFACE_TEXTURE_BUFFER,
#endif
#if defined(USE_OZONE)
    gfx::OZONE_NATIVE_BUFFER,
#endif
    gfx::SHARED_MEMORY_BUFFER
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<GpuMemoryBufferFactory> GpuMemoryBufferFactory::Create(
    gfx::GpuMemoryBufferType type) {
  switch (type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return make_scoped_ptr(new GpuMemoryBufferFactorySharedMemory);
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return make_scoped_ptr(new GpuMemoryBufferFactoryIOSurface);
#endif
#if defined(OS_ANDROID)
    case gfx::SURFACE_TEXTURE_BUFFER:
      return make_scoped_ptr(new GpuMemoryBufferFactorySurfaceTexture);
#endif
#if defined(USE_OZONE)
    case gfx::OZONE_NATIVE_BUFFER:
      return make_scoped_ptr(new GpuMemoryBufferFactoryOzoneNativeBuffer);
#endif
    default:
      NOTREACHED();
      return scoped_ptr<GpuMemoryBufferFactory>();
  }
}

}  // namespace content
