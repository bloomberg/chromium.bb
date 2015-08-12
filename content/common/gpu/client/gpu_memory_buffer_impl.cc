// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"

#if defined(OS_MACOSX)
#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/client/gpu_memory_buffer_impl_surface_texture.h"
#endif

#if defined(USE_OZONE)
#include "content/common/gpu/client/gpu_memory_buffer_impl_ozone_native_pixmap.h"
#endif

namespace content {

GpuMemoryBufferImpl::GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                                         const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         const DestructionCallback& callback)
    : id_(id),
      size_(size),
      format_(format),
      callback_(callback),
      mapped_(false),
      destruction_sync_point_(0) {}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
  DCHECK(!mapped_);
  callback_.Run(destruction_sync_point_);
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImpl::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
      return GpuMemoryBufferImplSharedMemory::CreateFromHandle(
          handle, size, format, callback);
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return GpuMemoryBufferImplIOSurface::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
#if defined(OS_ANDROID)
    case gfx::SURFACE_TEXTURE_BUFFER:
      return GpuMemoryBufferImplSurfaceTexture::CreateFromHandle(
          handle, size, format, callback);
#endif
#if defined(USE_OZONE)
    case gfx::OZONE_NATIVE_PIXMAP:
      return GpuMemoryBufferImplOzoneNativePixmap::CreateFromHandle(
          handle, size, format, usage, callback);
#endif
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
GpuMemoryBufferImpl* GpuMemoryBufferImpl::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<GpuMemoryBufferImpl*>(buffer);
}

// static
size_t GpuMemoryBufferImpl::SubsamplingFactor(gfx::BufferFormat format,
                                              int plane) {
  switch (format) {
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
      return 1;
    case gfx::BufferFormat::YUV_420: {
      static size_t factor[] = {1, 2, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(factor));
      return factor[plane];
    }
    case gfx::BufferFormat::YUV_420_BIPLANAR: {
      static size_t factor[] = {1, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(factor));
      return factor[plane];
    }
  }
  NOTREACHED();
  return 0;
}

// static
bool GpuMemoryBufferImpl::RowSizeInBytes(size_t width,
                                         gfx::BufferFormat format,
                                         int plane,
                                         size_t* size_in_bytes) {
  base::CheckedNumeric<size_t> checked_size = width;
  switch (format) {
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT5:
      DCHECK_EQ(plane, 0);
      *size_in_bytes = width;
      return true;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::ETC1:
      DCHECK_EQ(plane, 0);
      DCHECK_EQ(width % 2, 0u);
      *size_in_bytes = width / 2;
      return true;
    case gfx::BufferFormat::R_8:
      checked_size += 3;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = checked_size.ValueOrDie() & ~0x3;
      return true;
    case gfx::BufferFormat::RGBA_4444:
      checked_size *= 2;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = checked_size.ValueOrDie();
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_8888:
      checked_size *= 4;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = checked_size.ValueOrDie();
      return true;
    case gfx::BufferFormat::YUV_420:
      DCHECK_EQ(width % 2, 0u);
      *size_in_bytes = width / SubsamplingFactor(format, plane);
      return true;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      DCHECK_EQ(width % 2, 0u);
      *size_in_bytes = width;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool GpuMemoryBufferImpl::BufferSizeInBytes(const gfx::Size& size,
                                            gfx::BufferFormat format,
                                            size_t* size_in_bytes) {
  base::CheckedNumeric<size_t> checked_size = 0;
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
  for (size_t i = 0; i < num_planes; ++i) {
    size_t row_size_in_bytes = 0;
    if (!RowSizeInBytes(size.width(), format, i, &row_size_in_bytes))
      return false;
    base::CheckedNumeric<size_t> checked_plane_size = row_size_in_bytes;
    checked_plane_size *= size.height() / SubsamplingFactor(format, i);
    if (!checked_plane_size.IsValid())
      return false;
    checked_size += checked_plane_size.ValueOrDie();
    if (!checked_size.IsValid())
      return false;
  }
  *size_in_bytes = checked_size.ValueOrDie();
  return true;
}

gfx::BufferFormat GpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

bool GpuMemoryBufferImpl::IsMapped() const {
  return mapped_;
}

gfx::GpuMemoryBufferId GpuMemoryBufferImpl::GetId() const {
  return id_;
}

ClientBuffer GpuMemoryBufferImpl::AsClientBuffer() {
  return reinterpret_cast<ClientBuffer>(this);
}

}  // namespace content
