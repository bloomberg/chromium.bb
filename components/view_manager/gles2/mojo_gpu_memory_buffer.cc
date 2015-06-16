// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/gles2/mojo_gpu_memory_buffer.h"

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_conversions.h"

namespace gles2 {

namespace {

int NumberOfPlanesForGpuMemoryBufferFormat(
    gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::R_8:
    case gfx::GpuMemoryBuffer::RGBA_4444:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::BGRA_8888:
      return 1;
    case gfx::GpuMemoryBuffer::YUV_420:
      return 3;
  }
  NOTREACHED();
  return 0;
}

size_t SubsamplingFactor(gfx::GpuMemoryBuffer::Format format, int plane) {
  switch (format) {
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::DXT5:
    case gfx::GpuMemoryBuffer::ETC1:
    case gfx::GpuMemoryBuffer::R_8:
    case gfx::GpuMemoryBuffer::RGBA_4444:
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::BGRA_8888:
      return 1;
    case gfx::GpuMemoryBuffer::YUV_420: {
      static size_t factor[] = {1, 2, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(factor));
      return factor[plane];
    }
  }
  NOTREACHED();
  return 0;
}

size_t StrideInBytes(size_t width,
                     gfx::GpuMemoryBuffer::Format format,
                     int plane) {
  switch (format) {
    case gfx::GpuMemoryBuffer::ATCIA:
    case gfx::GpuMemoryBuffer::DXT5:
      DCHECK_EQ(plane, 0);
      return width;
    case gfx::GpuMemoryBuffer::ATC:
    case gfx::GpuMemoryBuffer::DXT1:
    case gfx::GpuMemoryBuffer::ETC1:
      DCHECK_EQ(plane, 0);
      DCHECK_EQ(width % 2, 0u);
      return width / 2;
    case gfx::GpuMemoryBuffer::R_8:
      return (width + 3) & ~0x3;
    case gfx::GpuMemoryBuffer::RGBA_4444:
      DCHECK_EQ(plane, 0);
      return width * 2;
    case gfx::GpuMemoryBuffer::RGBA_8888:
    case gfx::GpuMemoryBuffer::RGBX_8888:
    case gfx::GpuMemoryBuffer::BGRA_8888:
      DCHECK_EQ(plane, 0);
      return width * 4;
    case gfx::GpuMemoryBuffer::YUV_420:
      return width / SubsamplingFactor(format, plane);
  }
  NOTREACHED();
  return 0;
}

size_t BufferSizeInBytes(const gfx::Size& size,
                         gfx::GpuMemoryBuffer::Format format) {
  size_t size_in_bytes = 0;
  int num_planes = NumberOfPlanesForGpuMemoryBufferFormat(format);
  for (int i = 0; i < num_planes; ++i) {
    size_in_bytes += StrideInBytes(size.width(), format, i) *
                     (size.height() / SubsamplingFactor(format, i));
  }
  return size_in_bytes;
}

}  // namespace

MojoGpuMemoryBufferImpl::MojoGpuMemoryBufferImpl(
    const gfx::Size& size,
    Format format,
    scoped_ptr<base::SharedMemory> shared_memory)
    : size_(size),
      format_(format),
      shared_memory_(shared_memory.Pass()),
      mapped_(false) {
}

MojoGpuMemoryBufferImpl::~MojoGpuMemoryBufferImpl() {
}

scoped_ptr<gfx::GpuMemoryBuffer> MojoGpuMemoryBufferImpl::Create(
    const gfx::Size& size,
    Format format,
    Usage usage) {
  size_t bytes = BufferSizeInBytes(size, format);
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAnonymous(bytes))
    return nullptr;
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new MojoGpuMemoryBufferImpl(size, format, shared_memory.Pass()));
}

MojoGpuMemoryBufferImpl* MojoGpuMemoryBufferImpl::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<MojoGpuMemoryBufferImpl*>(buffer);
}

bool MojoGpuMemoryBufferImpl::Map(void** data) {
  DCHECK(!mapped_);
  if (!shared_memory_->Map(BufferSizeInBytes(size_, format_)))
    return false;
  mapped_ = true;
  size_t offset = 0;
  int num_planes = NumberOfPlanesForGpuMemoryBufferFormat(format_);
  for (int i = 0; i < num_planes; ++i) {
    data[i] = reinterpret_cast<uint8*>(shared_memory_->memory()) + offset;
    offset += StrideInBytes(size_.width(), format_, i) *
              (size_.height() / SubsamplingFactor(format_, i));
  }
  return true;
}

void MojoGpuMemoryBufferImpl::Unmap() {
  DCHECK(mapped_);
  shared_memory_->Unmap();
  mapped_ = false;
}

bool MojoGpuMemoryBufferImpl::IsMapped() const {
  return mapped_;
}

gfx::GpuMemoryBuffer::Format MojoGpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

void MojoGpuMemoryBufferImpl::GetStride(int* stride) const {
  int num_planes = NumberOfPlanesForGpuMemoryBufferFormat(format_);
  for (int i = 0; i < num_planes; ++i)
    stride[i] =
        base::checked_cast<int>(StrideInBytes(size_.width(), format_, i));
}

gfx::GpuMemoryBufferHandle MojoGpuMemoryBufferImpl::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = shared_memory_->handle();
  return handle;
}

ClientBuffer MojoGpuMemoryBufferImpl::AsClientBuffer() {
  return reinterpret_cast<ClientBuffer>(this);
}

}  // namespace gles2
