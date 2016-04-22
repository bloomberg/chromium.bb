// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GpuMemoryBufferFactory should
// pass in order to be conformant.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace gpu {

template <typename GpuMemoryBufferFactoryType>
class GpuMemoryBufferFactoryTest : public testing::Test {
 protected:
  GpuMemoryBufferFactoryType factory_;
};

TYPED_TEST_CASE_P(GpuMemoryBufferFactoryTest);

TYPED_TEST_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer) {
  const gfx::GpuMemoryBufferId kBufferId(1);
  const int kClientId = 1;

  gfx::Size buffer_size(2, 2);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ, gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT};
    for (auto usage : usages) {
      if (!IsNativeGpuMemoryBufferConfigurationSupported(format, usage))
        continue;

      gfx::GpuMemoryBufferHandle handle =
          TestFixture::factory_.CreateGpuMemoryBuffer(kBufferId, buffer_size,
                                                      format, usage, kClientId,
                                                      gpu::kNullSurfaceHandle);
      EXPECT_NE(handle.type, gfx::EMPTY_BUFFER);
      TestFixture::factory_.DestroyGpuMemoryBuffer(kBufferId, kClientId);
    }
  }
}

// The GpuMemoryBufferFactoryTest test case verifies behavior that is expected
// from a GpuMemoryBuffer factory in order to be conformant.
REGISTER_TYPED_TEST_CASE_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer);

template <typename GpuMemoryBufferFactoryType>
class GpuMemoryBufferFactoryImportTest
    : public GpuMemoryBufferFactoryTest<GpuMemoryBufferFactoryType> {};

TYPED_TEST_CASE_P(GpuMemoryBufferFactoryImportTest);

TYPED_TEST_P(GpuMemoryBufferFactoryImportTest,
             CreateGpuMemoryBufferFromHandle) {
  const int kClientId = 1;

  gfx::Size buffer_size(2, 2);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    if (!IsNativeGpuMemoryBufferConfigurationSupported(
            format, gfx::BufferUsage::GPU_READ)) {
      continue;
    }

    const gfx::GpuMemoryBufferId kBufferId1(1);
    gfx::GpuMemoryBufferHandle handle1 =
        TestFixture::factory_.CreateGpuMemoryBuffer(
            kBufferId1, buffer_size, format, gfx::BufferUsage::GPU_READ,
            kClientId, gpu::kNullSurfaceHandle);
    EXPECT_NE(handle1.type, gfx::EMPTY_BUFFER);

    // Create new buffer from |handle1|.
    const gfx::GpuMemoryBufferId kBufferId2(2);
    gfx::GpuMemoryBufferHandle handle2 =
        TestFixture::factory_.CreateGpuMemoryBufferFromHandle(
            handle1, kBufferId2, buffer_size, format, kClientId);
    EXPECT_NE(handle2.type, gfx::EMPTY_BUFFER);

    TestFixture::factory_.DestroyGpuMemoryBuffer(kBufferId1, kClientId);
    TestFixture::factory_.DestroyGpuMemoryBuffer(kBufferId2, kClientId);
  }
}

// The GpuMemoryBufferFactoryImportTest test case verifies that the
// GpuMemoryBufferFactory implementation handles import of buffers correctly.
REGISTER_TYPED_TEST_CASE_P(GpuMemoryBufferFactoryImportTest,
                           CreateGpuMemoryBufferFromHandle);

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
