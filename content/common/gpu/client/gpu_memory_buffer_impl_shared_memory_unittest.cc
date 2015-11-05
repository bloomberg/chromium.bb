// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_shared_memory.h"
#include "content/test/gpu_memory_buffer_impl_test_template.h"

namespace content {
namespace {

INSTANTIATE_TYPED_TEST_CASE_P(GpuMemoryBufferImplSharedMemory,
                              GpuMemoryBufferImplTest,
                              GpuMemoryBufferImplSharedMemory);

void BufferDestroyed(bool* destroyed, const gpu::SyncToken& sync_token) {
  *destroyed = true;
}

TEST(GpuMemoryBufferImplSharedMemoryTest, Create) {
  const gfx::GpuMemoryBufferId kBufferId(1);

  gfx::Size buffer_size(8, 8);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    bool destroyed = false;
    scoped_ptr<GpuMemoryBufferImplSharedMemory> buffer(
        GpuMemoryBufferImplSharedMemory::Create(
            kBufferId, buffer_size, format,
            base::Bind(&BufferDestroyed, base::Unretained(&destroyed))));
    ASSERT_TRUE(buffer);
    EXPECT_EQ(buffer->GetFormat(), format);

    // Check if destruction callback is executed when deleting the buffer.
    buffer.reset();
    ASSERT_TRUE(destroyed);
  }
}

}  // namespace
}  // namespace content
