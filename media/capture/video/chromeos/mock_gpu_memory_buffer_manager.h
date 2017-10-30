// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_GPU_MEMORY_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_GPU_MEMORY_BUFFER_MANAGER_H_

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/color_space.h"

namespace media {
namespace unittest_internal {

class MockGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  MockGpuMemoryBuffer();

  ~MockGpuMemoryBuffer() override;

  MOCK_METHOD0(Map, bool());

  MOCK_METHOD1(memory, void*(size_t plane));

  MOCK_METHOD0(Unmap, void());

  MOCK_CONST_METHOD0(GetSize, gfx::Size());

  MOCK_CONST_METHOD0(GetFormat, gfx::BufferFormat());

  MOCK_CONST_METHOD1(stride, int(size_t plane));

  MOCK_METHOD1(SetColorSpace, void(const gfx::ColorSpace& color_space));

  MOCK_CONST_METHOD0(GetId, gfx::GpuMemoryBufferId());

  MOCK_CONST_METHOD0(GetHandle, gfx::GpuMemoryBufferHandle());

  MOCK_METHOD0(AsClientBuffer, ClientBuffer());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGpuMemoryBuffer);
};

class MockGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  MockGpuMemoryBufferManager();

  ~MockGpuMemoryBufferManager() override;

  MOCK_METHOD4(
      CreateGpuMemoryBuffer,
      std::unique_ptr<gfx::GpuMemoryBuffer>(const gfx::Size& size,
                                            gfx::BufferFormat format,
                                            gfx::BufferUsage usage,
                                            gpu::SurfaceHandle surface_handle));

  MOCK_METHOD2(SetDestructionSyncToken,
               void(gfx::GpuMemoryBuffer* buffer,
                    const gpu::SyncToken& sync_token));

  std::unique_ptr<gfx::GpuMemoryBuffer> ReturnValidBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGpuMemoryBufferManager);
};

}  // namespace unittest_internal
}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_MOCK_GPU_MEMORY_BUFFER_MANAGER_H_
