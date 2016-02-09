// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"

#if defined(USE_OZONE)
#include <GLES2/gl2extchromium.h>
#include "components/exo/buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/aura/env.h"
#endif

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Display, public:

Display::Display() {}

Display::~Display() {}

scoped_ptr<Surface> Display::CreateSurface() {
  TRACE_EVENT0("exo", "Display::CreateSurface");

  return make_scoped_ptr(new Surface);
}

scoped_ptr<SharedMemory> Display::CreateSharedMemory(
    const base::SharedMemoryHandle& handle,
    size_t size) {
  TRACE_EVENT1("exo", "Display::CreateSharedMemory", "size", size);

  if (!base::SharedMemory::IsHandleValid(handle))
    return nullptr;

  return make_scoped_ptr(new SharedMemory(handle));
}

#if defined(USE_OZONE)
scoped_ptr<Buffer> Display::CreatePrimeBuffer(base::ScopedFD fd,
                                              const gfx::Size& size,
                                              gfx::BufferFormat format,
                                              int stride) {
  TRACE_EVENT1("exo", "Display::CreatePrimeBuffer", "size", size.ToString());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.native_pixmap_handle.fd = base::FileDescriptor(std::move(fd));
  handle.native_pixmap_handle.stride = stride;

  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBufferFromHandle(handle, size, format);
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer from handle";
    return nullptr;
  }

  // Using zero-copy for optimal performance.
  bool use_zero_copy = true;

  return make_scoped_ptr(
      new Buffer(std::move(gpu_memory_buffer), GL_TEXTURE_EXTERNAL_OES,
                 // COMMANDS_COMPLETED queries are required by native pixmaps.
                 GL_COMMANDS_COMPLETED_CHROMIUM, use_zero_copy));
}
#endif

scoped_ptr<ShellSurface> Display::CreateShellSurface(Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateShellSurface", "surface",
               surface->AsTracedValue());

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return make_scoped_ptr(new ShellSurface(surface));
}

scoped_ptr<SubSurface> Display::CreateSubSurface(Surface* surface,
                                                 Surface* parent) {
  TRACE_EVENT2("exo", "Display::CreateSubSurface", "surface",
               surface->AsTracedValue(), "parent", parent->AsTracedValue());

  if (surface->Contains(parent)) {
    DLOG(ERROR) << "Parent is contained within surface's hierarchy";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return make_scoped_ptr(new SubSurface(surface, parent));
}

}  // namespace exo
