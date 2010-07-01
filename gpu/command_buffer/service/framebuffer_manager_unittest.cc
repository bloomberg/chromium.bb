// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "app/gfx/gl/gl_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class FramebufferManagerTest : public testing::Test {
 public:
  FramebufferManagerTest() {
  }
  ~FramebufferManagerTest() {
    manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
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
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent framebuffer.
  EXPECT_TRUE(manager_.GetFramebufferInfo(kClient2Id) == NULL);
  // Check trying to a remove non-existent framebuffers does not crash.
  manager_.RemoveFramebufferInfo(kClient2Id);
  // Check we can't get the framebuffer after we remove it.
  manager_.RemoveFramebufferInfo(kClient1Id);
  EXPECT_TRUE(manager_.GetFramebufferInfo(kClient1Id) == NULL);
}

TEST_F(FramebufferManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create framebuffer.
  manager_.CreateFramebufferInfo(kClient1Id, kService1Id);
  // Check framebuffer got created.
  FramebufferManager::FramebufferInfo* info1 =
      manager_.GetFramebufferInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  info1 = manager_.GetFramebufferInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

// TODO(gman): Write test for AttachRenderbuffer

}  // namespace gles2
}  // namespace gpu


