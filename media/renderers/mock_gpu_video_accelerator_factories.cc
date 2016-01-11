// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/mock_gpu_video_accelerator_factories.h"

#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

bool gpu_memory_buffers_in_use_by_window_server = false;

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size, gfx::BufferFormat format)
      : mapped_(false),
        format_(format),
        size_(size),
        num_planes_(gfx::NumberOfPlanesForBufferFormat(format)) {
    DCHECK(gfx::BufferFormat::R_8 == format_ ||
           gfx::BufferFormat::YUV_420_BIPLANAR == format_ ||
           gfx::BufferFormat::UYVY_422 == format_);
    DCHECK(num_planes_ <= kMaxPlanes);
    for (int i = 0; i < static_cast<int>(num_planes_); ++i) {
      bytes_[i].resize(
          gfx::RowSizeForBufferFormat(size_.width(), format_, i) *
          size_.height() / gfx::SubsamplingFactorForBufferFormat(format_, i));
    }
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, num_planes_);
    return &bytes_[plane][0];
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  bool IsInUseByMacOSWindowServer() const override {
    return gpu_memory_buffers_in_use_by_window_server;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override {
    return format_;
  }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, num_planes_);
    return static_cast<int>(gfx::RowSizeForBufferFormat(
        size_.width(), format_, static_cast<int>(plane)));
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
  static const size_t kMaxPlanes = 3;

  bool mapped_;
  gfx::BufferFormat format_;
  const gfx::Size size_;
  size_t num_planes_;
  std::vector<uint8_t> bytes_[kMaxPlanes];
};

}  // unnamed namespace

MockGpuVideoAcceleratorFactories::MockGpuVideoAcceleratorFactories(
    gpu::gles2::GLES2Interface* gles2)
    : gles2_(gles2) {}

MockGpuVideoAcceleratorFactories::~MockGpuVideoAcceleratorFactories() {}

bool MockGpuVideoAcceleratorFactories::IsGpuVideoAcceleratorEnabled() {
  return true;
}

scoped_ptr<gfx::GpuMemoryBuffer>
MockGpuVideoAcceleratorFactories::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage /* usage */) {
  if (fail_to_allocate_gpu_memory_buffer_)
    return nullptr;
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(size, format));
}

void MockGpuVideoAcceleratorFactories::
    SetGpuMemoryBuffersInUseByMacOSWindowServer(bool in_use) {
  gpu_memory_buffers_in_use_by_window_server = in_use;
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

namespace {
class ScopedGLContextLockImpl
    : public GpuVideoAcceleratorFactories::ScopedGLContextLock {
 public:
  ScopedGLContextLockImpl(MockGpuVideoAcceleratorFactories* gpu_factories)
      : gpu_factories_(gpu_factories) {}
  gpu::gles2::GLES2Interface* ContextGL() override {
    return gpu_factories_->GetGLES2Interface();
  }

 private:
  MockGpuVideoAcceleratorFactories* gpu_factories_;
};
}  // namespace

scoped_ptr<GpuVideoAcceleratorFactories::ScopedGLContextLock>
MockGpuVideoAcceleratorFactories::GetGLContextLock() {
  DCHECK(gles2_);
  return make_scoped_ptr(new ScopedGLContextLockImpl(this));
}

}  // namespace media
