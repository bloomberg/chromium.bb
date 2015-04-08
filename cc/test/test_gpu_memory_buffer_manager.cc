// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

size_t NumberOfPlanesForGpuMemoryBufferFormat(
    gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::Format::ATC:
    case gfx::GpuMemoryBuffer::Format::ATCIA:
    case gfx::GpuMemoryBuffer::Format::DXT1:
    case gfx::GpuMemoryBuffer::Format::DXT5:
    case gfx::GpuMemoryBuffer::Format::ETC1:
    case gfx::GpuMemoryBuffer::Format::RGBA_8888:
    case gfx::GpuMemoryBuffer::Format::RGBX_8888:
    case gfx::GpuMemoryBuffer::Format::BGRA_8888:
      return 1;
    case gfx::GpuMemoryBuffer::Format::YUV_420:
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
      DCHECK_EQ(width % 2, 0U);
      return width / 2;
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
  size_t size_in_bytes = 0u;
  for (size_t i = 0; i < NumberOfPlanesForGpuMemoryBufferFormat(format); ++i) {
    size_in_bytes += StrideInBytes(size.width(), format, i) *
                     (size.height() / SubsamplingFactor(format, i));
  }
  return size_in_bytes;
}

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size,
                      Format format,
                      scoped_ptr<base::SharedMemory> shared_memory)
      : size_(size),
        format_(format),
        shared_memory_(shared_memory.Pass()),
        mapped_(false) {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map(void** data) override {
    DCHECK(!mapped_);
    if (!shared_memory_->Map(BufferSizeInBytes(size_, format_)))
      return false;
    mapped_ = true;
    data[0] = shared_memory_->memory();
    for (size_t i = 0; i < NumberOfPlanesForGpuMemoryBufferFormat(format_) - 1;
         ++i) {
      size_t offset = StrideInBytes(size_.width(), format_, i) *
                      (size_.height() / SubsamplingFactor(format_, i));
      data[i + 1] = reinterpret_cast<uint8*>(data[i]) + offset;
    }

    return true;
  }
  void Unmap() override {
    DCHECK(mapped_);
    shared_memory_->Unmap();
    mapped_ = false;
  }
  bool IsMapped() const override { return mapped_; }
  Format GetFormat() const override { return format_; }
  void GetStride(uint32* stride) const override {
    for (size_t i = 0; i < NumberOfPlanesForGpuMemoryBufferFormat(format_);
         ++i) {
      stride[i] = StrideInBytes(size_.width(), format_, i);
    }
  }
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    gfx::GpuMemoryBufferHandle handle;
    handle.type = gfx::SHARED_MEMORY_BUFFER;
    handle.handle = shared_memory_->handle();
    return handle;
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

 private:
  const gfx::Size size_;
  gfx::GpuMemoryBuffer::Format format_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  bool mapped_;
};

}  // namespace

TestGpuMemoryBufferManager::TestGpuMemoryBufferManager() {
}

TestGpuMemoryBufferManager::~TestGpuMemoryBufferManager() {
}

scoped_ptr<gfx::GpuMemoryBuffer>
TestGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAnonymous(BufferSizeInBytes(size, format)))
    return nullptr;
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(size, format, shared_memory.Pass()));
}

gfx::GpuMemoryBuffer*
TestGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
}

void TestGpuMemoryBufferManager::SetDestructionSyncPoint(
    gfx::GpuMemoryBuffer* buffer,
    uint32 sync_point) {
}

}  // namespace cc
