// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/service/gpu_memory_buffer_factory_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "gpu/ipc/service/gpu_memory_buffer_factory_ozone_native_pixmap.h"
#endif

namespace gpu {

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

}  // namespace gpu
