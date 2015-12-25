// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/gpu_memory_buffer_factory_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/gpu_memory_buffer_factory_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"
#endif

namespace content {

// static
gfx::GpuMemoryBufferType GpuMemoryBufferFactory::GetNativeType() {
#if defined(OS_MACOSX)
  return gfx::IO_SURFACE_BUFFER;
#endif
#if defined(OS_ANDROID)
  return gfx::SURFACE_TEXTURE_BUFFER;
#endif
#if defined(USE_OZONE)
  return gfx::OZONE_NATIVE_PIXMAP;
#endif
  return gfx::EMPTY_BUFFER;
}

// static
scoped_ptr<GpuMemoryBufferFactory> GpuMemoryBufferFactory::CreateNativeType() {
#if defined(OS_MACOSX)
  return make_scoped_ptr(new GpuMemoryBufferFactoryIOSurface);
#endif
#if defined(OS_ANDROID)
  return make_scoped_ptr(new GpuMemoryBufferFactorySurfaceTexture);
#endif
#if defined(USE_OZONE)
  return make_scoped_ptr(new GpuMemoryBufferFactoryOzoneNativePixmap);
#endif
  NOTREACHED();
  return nullptr;
}

}  // namespace content
