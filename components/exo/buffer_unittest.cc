// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "cc/resources/single_release_callback.h"
#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

using BufferTest = test::ExoTestBase;

void Release(int* release_call_count) {
  (*release_call_count)++;
}

TEST_F(BufferTest, ReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  scoped_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size).Pass(),
                 GL_TEXTURE_2D));

  // Set the release callback.
  int release_call_count = 0;
  buffer->set_release_callback(
      base::Bind(&Release, base::Unretained(&release_call_count)));

  // Acquire a texture mailbox for the contents of the buffer.
  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> buffer_release_callback =
      buffer->AcquireTextureMailbox(&texture_mailbox);
  ASSERT_TRUE(buffer_release_callback);

  // Trying to acquire an already in-use buffer should fail.
  EXPECT_FALSE(buffer->AcquireTextureMailbox(&texture_mailbox));

  // Release buffer.
  buffer_release_callback->Run(gpu::SyncToken(), false);

  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

}  // namespace
}  // namespace exo
