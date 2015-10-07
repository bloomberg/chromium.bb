// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GpuMemoryBufferFactory should
// pass in order to be conformant.

#ifndef CONTENT_TEST_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
#define CONTENT_TEST_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_

#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace content {

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

  for (auto format : gfx::GetBufferFormats()) {
    gfx::BufferUsage usages[] = {gfx::BufferUsage::MAP,
                                 gfx::BufferUsage::PERSISTENT_MAP,
                                 gfx::BufferUsage::SCANOUT};
    for (auto usage : usages) {
      if (!TypeParam::IsGpuMemoryBufferConfigurationSupported(format, usage))
        continue;

      gfx::GpuMemoryBufferHandle handle =
          TestFixture::factory_.CreateGpuMemoryBuffer(kBufferId, buffer_size,
                                                      format, usage, kClientId,
                                                      gfx::kNullPluginWindow);
      EXPECT_NE(handle.type, gfx::EMPTY_BUFFER);
      TestFixture::factory_.DestroyGpuMemoryBuffer(kBufferId, kClientId);
    }
  }
}

// The GpuMemoryBufferFactoryTest test case verifies behavior that is expected
// from a GpuMemoryBuffer factory in order to be conformant.
REGISTER_TYPED_TEST_CASE_P(GpuMemoryBufferFactoryTest, CreateGpuMemoryBuffer);

}  // namespace content

#endif  // CONTENT_TEST_GPU_MEMORY_BUFFER_FACTORY_TEST_TEMPLATE_H_
