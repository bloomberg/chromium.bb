// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size, Format format)
      : size_(size),
        format_(format),
        pixels_(new uint8[size.GetArea() * BytesPerPixel(format)]),
        mapped_(false) {}

  // Overridden from gfx::GpuMemoryBuffer:
  void* Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return pixels_.get();
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  bool IsMapped() const override { return mapped_; }
  Format GetFormat() const override { return format_; }
  uint32 GetStride() const override {
    return size_.width() * BytesPerPixel(format_);
  }
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

 private:
  static size_t BytesPerPixel(Format format) {
    switch (format) {
      case RGBA_8888:
      case RGBX_8888:
      case BGRA_8888:
        return 4;
    }

    NOTREACHED();
    return 0;
  }

  const gfx::Size size_;
  gfx::GpuMemoryBuffer::Format format_;
  scoped_ptr<uint8[]> pixels_;
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
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(size, format));
}

gfx::GpuMemoryBuffer*
TestGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
}

}  // namespace cc
