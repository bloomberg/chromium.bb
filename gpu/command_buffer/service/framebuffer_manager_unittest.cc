// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class FramebufferManagerTest : public testing::Test {
 public:
  FramebufferManagerTest() {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  FramebufferManager manager_;
};

TEST_F(FramebufferManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  // Check we can create framebuffer.
  manager_.CreateFramebufferInfo(kClient1Id, kService1Id);
  // Check framebuffer got created.
  FramebufferManager::FramebufferInfo* info1 =
      manager_.GetFramebufferInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_FALSE(info1->IsDeleted());
  EXPECT_EQ(kService1Id, info1->service_id());
  // Check we get nothing for a non-existent framebuffer.
  EXPECT_TRUE(manager_.GetFramebufferInfo(kClient2Id) == NULL);
  // Check trying to a remove non-existent framebuffers does not crash.
  manager_.RemoveFramebufferInfo(kClient2Id);
  // Check we can't get the framebuffer after we remove it.
  manager_.RemoveFramebufferInfo(kClient1Id);
  EXPECT_TRUE(manager_.GetFramebufferInfo(kClient1Id) == NULL);
}

// TODO(gman): Write test for AttachRenderbuffer

}  // namespace gles2
}  // namespace gpu


