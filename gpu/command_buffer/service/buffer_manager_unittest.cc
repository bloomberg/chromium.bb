// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class BufferManagerTest : public testing::Test {
 public:
  BufferManagerTest() {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  BufferManager manager_;
};

TEST_F(BufferManagerTest, Basic) {
  const GLuint kBuffer1Id = 1;
  const GLsizeiptr kBuffer1Size = 123;
  const GLuint kBuffer2Id = 2;
  // Check we can create buffer.
  manager_.CreateBufferInfo(kBuffer1Id);
  // Check buffer got created.
  BufferManager::BufferInfo* info1 = manager_.GetBufferInfo(kBuffer1Id);
  ASSERT_TRUE(info1 != NULL);
  // Check we and set its size.
  info1->set_size(kBuffer1Size);
  EXPECT_EQ(kBuffer1Size, info1->size());
  // Check we get nothing for a non-existent buffer.
  EXPECT_TRUE(manager_.GetBufferInfo(kBuffer2Id) == NULL);
  // Check trying to a remove non-existent buffers does not crash.
  manager_.RemoveBufferInfo(kBuffer2Id);
  // Check we can't get the buffer after we remove it.
  manager_.RemoveBufferInfo(kBuffer1Id);
  EXPECT_TRUE(manager_.GetBufferInfo(kBuffer1Id) == NULL);
}

// TODO(gman): Test GetMaxValueForRange.

}  // namespace gles2
}  // namespace gpu


