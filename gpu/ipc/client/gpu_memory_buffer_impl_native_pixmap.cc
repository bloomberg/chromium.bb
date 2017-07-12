// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/gpu_memory_buffer_impl_native_pixmap.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace gpu {
namespace {

void FreeNativePixmapForTesting(
    scoped_refptr<gfx::NativePixmap> native_pixmap) {
  // Nothing to do here. |native_pixmap| will be freed when this function
  // returns and reference count drops to 0.
}

}  // namespace

GpuMemoryBufferImplNativePixmap::GpuMemoryBufferImplNativePixmap(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    std::unique_ptr<gfx::ClientNativePixmap> pixmap,
    const std::vector<gfx::NativePixmapPlane>& planes,
    base::ScopedFD fd)
    : GpuMemoryBufferImpl(id, size, format, callback),
      pixmap_(std::move(pixmap)),
      planes_(planes),
      fd_(std::move(fd)) {}

GpuMemoryBufferImplNativePixmap::~GpuMemoryBufferImplNativePixmap() {}

// static
std::unique_ptr<GpuMemoryBufferImplNativePixmap>
GpuMemoryBufferImplNativePixmap::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  // GpuMemoryBufferImpl needs the FD to implement GetHandle() but
  // gfx::ClientNativePixmapFactory::ImportFromHandle is expected to take
  // ownership of the FD passed in the handle so we have to dup it here in
  // order to pass a valid FD to the GpuMemoryBufferImpl ctor.
  base::ScopedFD scoped_fd;
  if (!handle.native_pixmap_handle.fds.empty()) {
    scoped_fd.reset(HANDLE_EINTR(dup(handle.native_pixmap_handle.fds[0].fd)));
    if (!scoped_fd.is_valid()) {
      PLOG(ERROR) << "dup";
      return nullptr;
    }
  }

  gfx::NativePixmapHandle native_pixmap_handle;
  if (scoped_fd.is_valid()) {
    native_pixmap_handle.fds.emplace_back(handle.native_pixmap_handle.fds[0].fd,
                                          true /* auto_close */);
  }
  native_pixmap_handle.planes = handle.native_pixmap_handle.planes;
  std::unique_ptr<gfx::ClientNativePixmap> native_pixmap =
      gfx::ClientNativePixmapFactory::GetInstance()->ImportFromHandle(
          native_pixmap_handle, size, usage);
  DCHECK(native_pixmap);

  return base::WrapUnique(new GpuMemoryBufferImplNativePixmap(
      handle.id, size, format, callback, std::move(native_pixmap),
      handle.native_pixmap_handle.planes, std::move(scoped_fd)));
}

// static
bool GpuMemoryBufferImplNativePixmap::IsConfigurationSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return gpu::IsNativeGpuMemoryBufferConfigurationSupported(format, usage);
}

// static
base::Closure GpuMemoryBufferImplNativePixmap::AllocateForTesting(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::GpuMemoryBufferHandle* handle) {
  DCHECK(IsConfigurationSupported(format, usage));
#if defined(USE_OZONE)
  scoped_refptr<gfx::NativePixmap> pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(gfx::kNullAcceleratedWidget, size, format,
                               usage);
  handle->native_pixmap_handle = pixmap->ExportHandle();
#else
  // TODO(j.isorce): use gbm_bo_create / gbm_bo_get_fd from system libgbm.
  scoped_refptr<gfx::NativePixmap> pixmap;
  NOTIMPLEMENTED();
#endif
  handle->type = gfx::NATIVE_PIXMAP;
  return base::Bind(&FreeNativePixmapForTesting, pixmap);
}

bool GpuMemoryBufferImplNativePixmap::Map() {
  DCHECK(!mapped_);
  mapped_ = pixmap_->Map();
  return mapped_;
}

void* GpuMemoryBufferImplNativePixmap::memory(size_t plane) {
  DCHECK(mapped_);
  return pixmap_->GetMemoryAddress(plane);
}

void GpuMemoryBufferImplNativePixmap::Unmap() {
  DCHECK(mapped_);
  pixmap_->Unmap();
  mapped_ = false;
}

int GpuMemoryBufferImplNativePixmap::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return pixmap_->GetStride(plane);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplNativePixmap::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  handle.id = id_;
  if (fd_.is_valid()) {
    handle.native_pixmap_handle.fds.emplace_back(fd_.get(),
                                                 false /* auto_close */);
  }
  handle.native_pixmap_handle.planes = planes_;
  return handle;
}

}  // namespace gpu
