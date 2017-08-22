// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mock_gpu_memory_buffer_manager.h"

#include "base/memory/ptr_util.h"

using ::testing::Return;

namespace media {
namespace unittest_internal {

MockGpuMemoryBuffer::MockGpuMemoryBuffer() {}

MockGpuMemoryBuffer::~MockGpuMemoryBuffer() {}

MockGpuMemoryBufferManager::MockGpuMemoryBufferManager() {}

MockGpuMemoryBufferManager::~MockGpuMemoryBufferManager() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
MockGpuMemoryBufferManager::ReturnValidBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  // We use only NV12 in unit tests.
  EXPECT_EQ(gfx::BufferFormat::YUV_420_BIPLANAR, format);

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  // Set a dummy id since this is for testing only.
  handle.id = gfx::GpuMemoryBufferId(0);
  // Set a dummy fd since this is for testing only.
  handle.native_pixmap_handle.fds.push_back(base::FileDescriptor(0, false));
  handle.native_pixmap_handle.planes.push_back(
      gfx::NativePixmapPlane(size.width(), 0, size.width() * size.height(), 0));
  handle.native_pixmap_handle.planes.push_back(gfx::NativePixmapPlane(
      size.width(), handle.native_pixmap_handle.planes[0].size,
      size.width() * size.height() / 2, 0));

  auto mock_buffer = base::MakeUnique<MockGpuMemoryBuffer>();
  ON_CALL(*mock_buffer, Map()).WillByDefault(Return(true));
  ON_CALL(*mock_buffer, memory(0))
      .WillByDefault(Return(reinterpret_cast<void*>(0xdeafbeef)));
  ON_CALL(*mock_buffer, GetHandle()).WillByDefault(Return(handle));

  return mock_buffer;
}

}  // namespace unittest_internal
}  // namespace media
