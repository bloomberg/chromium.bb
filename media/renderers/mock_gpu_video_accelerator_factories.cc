// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/mock_gpu_video_accelerator_factories.h"

#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size, gfx::BufferFormat format)
      : format_(format), size_(size) {
    DCHECK(gfx::BufferFormat::R_8 == format_ ||
           gfx::BufferFormat::UYVY_422 == format_);
    bytes_.resize(size_.GetArea() *
                  (format_ == gfx::BufferFormat::UYVY_422 ? 2 : 1));
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map(void** data) override {
    data[0] = &bytes_[0];
    return true;
  }
  void Unmap() override{};
  bool IsMapped() const override {
    NOTREACHED();
    return false;
  }
  gfx::BufferFormat GetFormat() const override {
    NOTREACHED();
    return gfx::BufferFormat::R_8;
  }
  void GetStride(int* stride) const override {
    stride[0] =
        size_.width() * (format_ == gfx::BufferFormat::UYVY_422 ? 2 : 1);
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

 private:
  gfx::BufferFormat format_;
  std::vector<unsigned char> bytes_;
  const gfx::Size size_;
};

}  // unnamed namespace

MockGpuVideoAcceleratorFactories::MockGpuVideoAcceleratorFactories() {}

MockGpuVideoAcceleratorFactories::~MockGpuVideoAcceleratorFactories() {}

bool MockGpuVideoAcceleratorFactories::IsGpuVideoAcceleratorEnabled() {
  return true;
}

scoped_ptr<gfx::GpuMemoryBuffer>
MockGpuVideoAcceleratorFactories::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(size, format));
}

scoped_ptr<base::SharedMemory>
MockGpuVideoAcceleratorFactories::CreateSharedMemory(size_t size) {
  return nullptr;
}

scoped_ptr<VideoDecodeAccelerator>
MockGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator() {
  return scoped_ptr<VideoDecodeAccelerator>(DoCreateVideoDecodeAccelerator());
}

scoped_ptr<VideoEncodeAccelerator>
MockGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return scoped_ptr<VideoEncodeAccelerator>(DoCreateVideoEncodeAccelerator());
}

bool MockGpuVideoAcceleratorFactories::ShouldUseGpuMemoryBuffersForVideoFrames()
    const {
  return false;
}

unsigned MockGpuVideoAcceleratorFactories::ImageTextureTarget() {
  return GL_TEXTURE_2D;
}

}  // namespace media
