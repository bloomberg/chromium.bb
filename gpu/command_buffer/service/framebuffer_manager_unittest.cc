// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include gtest.h out of order because <X11/X.h> #define's Bool & None, which
// gtest uses as struct names (inside a namespace).  This means that
// #include'ing gtest after anything that pulls in X.h fails to compile.
// This is http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/feature_info.h"

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
  EXPECT_TRUE(NULL == info_->GetAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(NULL == info_->GetAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(NULL == info_->GetAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_TRUE(NULL == info_->GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  EXPECT_FALSE(info_->HasDepthAttachment());
  EXPECT_FALSE(info_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(0), info_->GetColorAttachmentFormat());
}

TEST_F(FramebufferInfoTest, AttachRenderbuffer) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;
  const GLint kMaxRenderbufferSize = 128;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLenum kFormat1 = GL_STENCIL_INDEX8;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 64;
  const GLsizei kHeight2 = 128;
  const GLenum kFormat2 = GL_STENCIL_INDEX;
  const GLsizei kSamples2 = 0;
  const GLsizei kWidth3 = 75;
  const GLsizei kHeight3 = 123;
  const GLenum kFormat3 = GL_STENCIL_INDEX8;
  const GLsizei kSamples3 = 0;

  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  EXPECT_FALSE(info_->IsNotComplete());

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
  EXPECT_TRUE(info_->IsNotComplete());
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA4), info_->GetColorAttachmentFormat());
  EXPECT_FALSE(info_->HasDepthAttachment());
  EXPECT_FALSE(info_->HasStencilAttachment());

  rb_info1->SetInfo(1, GL_RGB, 0, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_RGB), info_->GetColorAttachmentFormat());
  EXPECT_FALSE(info_->HasDepthAttachment());
  EXPECT_FALSE(info_->HasStencilAttachment());

  // check adding another
  info_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, rb_info1);
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(info_->IsNotComplete());
  EXPECT_EQ(static_cast<GLenum>(GL_RGB), info_->GetColorAttachmentFormat());
  EXPECT_TRUE(info_->HasDepthAttachment());
  EXPECT_FALSE(info_->HasStencilAttachment());

  // check marking them as cleared.
  info_->MarkAttachedRenderbuffersAsCleared();
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(info_->IsNotComplete());

  // Check adding one that is already cleared.
  info_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, rb_info1);
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_RGB), info_->GetColorAttachmentFormat());
  EXPECT_TRUE(info_->HasDepthAttachment());
  EXPECT_TRUE(info_->HasStencilAttachment());

  // Check marking the renderbuffer as unclared.
  rb_info1->SetInfo(kSamples1, kFormat1, kWidth1, kHeight1);
  EXPECT_FALSE(info_->IsNotComplete());
  EXPECT_EQ(static_cast<GLenum>(kFormat1), info_->GetColorAttachmentFormat());
  EXPECT_TRUE(info_->HasDepthAttachment());
  EXPECT_TRUE(info_->HasStencilAttachment());

  const FramebufferManager::FramebufferInfo::Attachment* attachment =
      info_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth1, attachment->width());
  EXPECT_EQ(kHeight1, attachment->height());
  EXPECT_EQ(kSamples1, attachment->samples());
  EXPECT_EQ(kFormat1, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());

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
  rb_info2->SetInfo(kSamples2, kFormat2, kWidth2, kHeight2);

  info_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, rb_info2);
  EXPECT_TRUE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  attachment = info_->GetAttachment(GL_STENCIL_ATTACHMENT);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth2, attachment->width());
  EXPECT_EQ(kHeight2, attachment->height());
  EXPECT_EQ(kSamples2, attachment->samples());
  EXPECT_EQ(kFormat2, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());

  // Check changing an attachment.
  rb_info2->SetInfo(kSamples3, kFormat3, kWidth3, kHeight3);

  attachment = info_->GetAttachment(GL_STENCIL_ATTACHMENT);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth3, attachment->width());
  EXPECT_EQ(kHeight3, attachment->height());
  EXPECT_EQ(kSamples3, attachment->samples());
  EXPECT_EQ(kFormat3, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());

  // Check removing it.
  info_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, NULL);
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1), info_->GetColorAttachmentFormat());
  EXPECT_TRUE(info_->HasDepthAttachment());
  EXPECT_FALSE(info_->HasStencilAttachment());

  rb_manager.Destroy(false);
}

TEST_F(FramebufferInfoTest, AttachTexture) {
  const GLuint kTextureClient1Id = 33;
  const GLuint kTextureService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLint kMaxTextureSize = 128;
  const GLint kDepth = 1;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLint kLevel1 = 0;
  const GLenum kFormat1 = GL_RGBA;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 64;
  const GLsizei kHeight2 = 128;
  const GLint kLevel2 = 0;
  const GLenum kFormat2 = GL_RGB;
  const GLenum kTarget2 = GL_TEXTURE_2D;
  const GLsizei kSamples2 = 0;
  const GLsizei kWidth3 = 75;
  const GLsizei kHeight3 = 123;
  const GLint kLevel3 = 0;
  const GLenum kFormat3 = GL_RGB565;
  const GLsizei kSamples3 = 0;
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  EXPECT_FALSE(info_->IsNotComplete());

  FeatureInfo feature_info;
  TextureManager tex_manager(kMaxTextureSize, kMaxTextureSize);
  tex_manager.CreateTextureInfo(
      &feature_info, kTextureClient1Id, kTextureService1Id);
  TextureManager::TextureInfo* tex_info1 =
      tex_manager.GetTextureInfo(kTextureClient1Id);
  ASSERT_TRUE(tex_info1 != NULL);

  // check adding one attachment
  info_->AttachTexture(GL_COLOR_ATTACHMENT0, tex_info1, kTarget1, kLevel1);
  EXPECT_FALSE(info_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(info_->IsNotComplete());
  EXPECT_EQ(static_cast<GLenum>(0), info_->GetColorAttachmentFormat());

  tex_manager.SetInfoTarget(tex_info1, GL_TEXTURE_2D);
  tex_manager.SetLevelInfo(
      &feature_info, tex_info1, GL_TEXTURE_2D, kLevel1,
      kFormat1, kWidth1, kHeight1, kDepth, kBorder, kFormat1, kType);
  EXPECT_FALSE(info_->IsNotComplete());
  EXPECT_EQ(static_cast<GLenum>(kFormat1), info_->GetColorAttachmentFormat());

  const FramebufferManager::FramebufferInfo::Attachment* attachment =
      info_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth1, attachment->width());
  EXPECT_EQ(kHeight1, attachment->height());
  EXPECT_EQ(kSamples1, attachment->samples());
  EXPECT_EQ(kFormat1, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());

  // Check replacing an attachment
  tex_manager.CreateTextureInfo(
      &feature_info, kTextureClient2Id, kTextureService2Id);
  TextureManager::TextureInfo* tex_info2 =
      tex_manager.GetTextureInfo(kTextureClient2Id);
  ASSERT_TRUE(tex_info2 != NULL);
  tex_manager.SetInfoTarget(tex_info2, GL_TEXTURE_2D);
  tex_manager.SetLevelInfo(
      &feature_info, tex_info2, GL_TEXTURE_2D, kLevel2,
      kFormat2, kWidth2, kHeight2, kDepth, kBorder, kFormat2, kType);

  info_->AttachTexture(GL_COLOR_ATTACHMENT0, tex_info2, kTarget2, kLevel2);
  EXPECT_EQ(static_cast<GLenum>(kFormat2), info_->GetColorAttachmentFormat());

  attachment = info_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth2, attachment->width());
  EXPECT_EQ(kHeight2, attachment->height());
  EXPECT_EQ(kSamples2, attachment->samples());
  EXPECT_EQ(kFormat2, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());

  // Check changing attachment
  tex_manager.SetLevelInfo(
      &feature_info, tex_info2, GL_TEXTURE_2D, kLevel3,
      kFormat3, kWidth3, kHeight3, kDepth, kBorder, kFormat3, kType);
  attachment = info_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth3, attachment->width());
  EXPECT_EQ(kHeight3, attachment->height());
  EXPECT_EQ(kSamples3, attachment->samples());
  EXPECT_EQ(kFormat3, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat3), info_->GetColorAttachmentFormat());

  // Check removing it.
  info_->AttachTexture(GL_COLOR_ATTACHMENT0, NULL, 0, 0);
  EXPECT_TRUE(info_->GetAttachment(GL_COLOR_ATTACHMENT0) == NULL);
  EXPECT_EQ(static_cast<GLenum>(0), info_->GetColorAttachmentFormat());

  tex_manager.Destroy(false);
}

}  // namespace gles2
}  // namespace gpu
