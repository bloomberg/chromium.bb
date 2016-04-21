// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"

#include <iterator>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "ui/views/widget/widget.h"

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

std::unique_ptr<Surface> Display::CreateSurface() {
  TRACE_EVENT0("exo", "Display::CreateSurface");

  return base::WrapUnique(new Surface);
}

std::unique_ptr<SharedMemory> Display::CreateSharedMemory(
    const base::SharedMemoryHandle& handle,
    size_t size) {
  TRACE_EVENT1("exo", "Display::CreateSharedMemory", "size", size);

  if (!base::SharedMemory::IsHandleValid(handle))
    return nullptr;

  return base::WrapUnique(new SharedMemory(handle));
}

#if defined(USE_OZONE)
std::unique_ptr<Buffer> Display::CreateLinuxDMABufBuffer(
    base::ScopedFD fd,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int stride) {
  TRACE_EVENT1("exo", "Display::CreateLinuxDMABufBuffer", "size",
               size.ToString());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  handle.native_pixmap_handle.fd = base::FileDescriptor(std::move(fd));
  handle.native_pixmap_handle.stride = stride;

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
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

  // List of overlay formats that are known to be supported.
  // TODO(reveman): Determine this at runtime.
  const gfx::BufferFormat kOverlayFormats[] = {gfx::BufferFormat::BGRX_8888};
  bool is_overlay_candidate =
      std::find(std::begin(kOverlayFormats), std::end(kOverlayFormats),
                format) != std::end(kOverlayFormats);

  return base::WrapUnique(new Buffer(
      std::move(gpu_memory_buffer), GL_TEXTURE_EXTERNAL_OES,
      // COMMANDS_COMPLETED queries are required by native pixmaps.
      GL_COMMANDS_COMPLETED_CHROMIUM, use_zero_copy, is_overlay_candidate));
}
#endif

std::unique_ptr<ShellSurface> Display::CreateShellSurface(Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateShellSurface", "surface",
               surface->AsTracedValue());

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return base::WrapUnique(
      new ShellSurface(surface, nullptr, gfx::Rect(), true));
}

std::unique_ptr<ShellSurface> Display::CreatePopupShellSurface(
    Surface* surface,
    ShellSurface* parent,
    const gfx::Point& position) {
  TRACE_EVENT2("exo", "Display::CreatePopupShellSurface", "surface",
               surface->AsTracedValue(), "parent", parent->AsTracedValue());

  if (surface->Contains(parent->GetWidget()->GetNativeWindow())) {
    DLOG(ERROR) << "Parent is contained within surface's hierarchy";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return base::WrapUnique(new ShellSurface(
      surface, parent, gfx::Rect(position, gfx::Size(1, 1)), false));
}

std::unique_ptr<SubSurface> Display::CreateSubSurface(Surface* surface,
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

  return base::WrapUnique(new SubSurface(surface, parent));
}

}  // namespace exo
