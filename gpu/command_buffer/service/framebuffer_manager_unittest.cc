// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/framebuffer_manager.h"

#include "gpu/command_buffer/common/gl_mock.h"
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

class FramebufferInfoTest : public testing::Test {
 public:
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;

  FramebufferInfoTest()
      : manager_() {
  }
  ~FramebufferInfoTest() {
    manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    manager_.CreateFramebufferInfo(kClient1Id, kService1Id);
    info_ = manager_.GetFramebufferInfo(kClient1Id);
    ASSERT_TRUE(info_ != NULL);
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  FramebufferManager manager_;
  FramebufferManager::FramebufferInfo* info_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint FramebufferInfoTest::kClient1Id;
const GLuint FramebufferInfoTest::kService1Id;
#endif

TEST_F(FramebufferInfoTest, Basic) {
  EXPECT_EQ(kService1Id, info_->service_id());
  EXPECT_FALSE(info_->IsDeleted());
}

TEST_F(FramebufferInfoTest, AttachRenderbuffer) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;
  const GLint kMaxRenderbufferSize = 128;
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT));

  RenderbufferManager rb_manager(kMaxRenderbufferSize);
  rb_manager.CreateRenderbufferInfo(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  RenderbufferManager::RenderbufferInfo* rb_info1 =
      rb_manager.GetRenderbufferInfo(kRenderbufferClient1Id);
  ASSERT_TRUE(rb_info1 != NULL);

  // check adding one attachment
  info_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, rb_info1);
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));

  // check adding another
  info_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, rb_info1);
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));

  // check marking them as cleared.
  info_->MarkAttachedRenderbuffersAsCleared();
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));

  // Check adding one that is already cleared.
  info_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, rb_info1);
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  // Check marking the renderbuffer as unclared.
  rb_info1->set_internal_format(GL_RGBA);
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  // Clear it.
  info_->MarkAttachedRenderbuffersAsCleared();
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  // Check replacing an attachment
  rb_manager.CreateRenderbufferInfo(
      kRenderbufferClient2Id, kRenderbufferService2Id);
  RenderbufferManager::RenderbufferInfo* rb_info2 =
      rb_manager.GetRenderbufferInfo(kRenderbufferClient2Id);
  ASSERT_TRUE(rb_info2 != NULL);

  info_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, rb_info2);
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  // Check removing it.
  info_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, NULL);
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  rb_manager.Destroy(false);
}

}  // namespace gles2
}  // namespace gpu


