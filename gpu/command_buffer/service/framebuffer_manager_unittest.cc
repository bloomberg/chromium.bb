// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/error_state_mock.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::Return;

namespace gpu {
namespace gles2 {

class FramebufferManagerTest : public testing::Test {
 static const GLint kMaxTextureSize = 64;
 static const GLint kMaxCubemapSize = 64;
 static const GLint kMaxRenderbufferSize = 64;
 static const GLint kMaxSamples = 4;

 public:
  FramebufferManagerTest()
      : manager_(1, 1),
        texture_manager_(
          NULL, new FeatureInfo(), kMaxTextureSize, kMaxCubemapSize),
        renderbuffer_manager_(NULL, kMaxRenderbufferSize, kMaxSamples) {

  }
  virtual ~FramebufferManagerTest() {
    manager_.Destroy(false);
    texture_manager_.Destroy(false);
    renderbuffer_manager_.Destroy(false);
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
  TextureManager texture_manager_;
  RenderbufferManager renderbuffer_manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint FramebufferManagerTest::kMaxTextureSize;
const GLint FramebufferManagerTest::kMaxCubemapSize;
const GLint FramebufferManagerTest::kMaxRenderbufferSize;
#endif

TEST_F(FramebufferManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  // Check we can create framebuffer.
  manager_.CreateFramebuffer(kClient1Id, kService1Id);
  // Check framebuffer got created.
  Framebuffer* framebuffer1 = manager_.GetFramebuffer(kClient1Id);
  ASSERT_TRUE(framebuffer1 != NULL);
  EXPECT_FALSE(framebuffer1->IsDeleted());
  EXPECT_EQ(kService1Id, framebuffer1->service_id());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(framebuffer1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent framebuffer.
  EXPECT_TRUE(manager_.GetFramebuffer(kClient2Id) == NULL);
  // Check trying to a remove non-existent framebuffers does not crash.
  manager_.RemoveFramebuffer(kClient2Id);
  // Check framebuffer gets deleted when last reference is released.
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  // Check we can't get the framebuffer after we remove it.
  manager_.RemoveFramebuffer(kClient1Id);
  EXPECT_TRUE(manager_.GetFramebuffer(kClient1Id) == NULL);
}

TEST_F(FramebufferManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create framebuffer.
  manager_.CreateFramebuffer(kClient1Id, kService1Id);
  // Check framebuffer got created.
  Framebuffer* framebuffer1 = manager_.GetFramebuffer(kClient1Id);
  ASSERT_TRUE(framebuffer1 != NULL);
  EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  framebuffer1 = manager_.GetFramebuffer(kClient1Id);
  ASSERT_TRUE(framebuffer1 == NULL);
}

class FramebufferInfoTest : public testing::Test {
 public:
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;

  static const GLint kMaxTextureSize = 64;
  static const GLint kMaxCubemapSize = 64;
  static const GLint kMaxRenderbufferSize = 64;
  static const GLint kMaxSamples = 4;

  FramebufferInfoTest()
      : manager_(1, 1),
        texture_manager_(
          NULL, new FeatureInfo(), kMaxTextureSize, kMaxCubemapSize),
        renderbuffer_manager_(NULL, kMaxRenderbufferSize, kMaxSamples) {
  }
  virtual ~FramebufferInfoTest() {
    manager_.Destroy(false);
    texture_manager_.Destroy(false);
    renderbuffer_manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    manager_.CreateFramebuffer(kClient1Id, kService1Id);
    error_state_.reset(new ::testing::StrictMock<gles2::MockErrorState>());
    framebuffer_ = manager_.GetFramebuffer(kClient1Id);
    ASSERT_TRUE(framebuffer_ != NULL);
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  FramebufferManager manager_;
  Framebuffer* framebuffer_;
  TextureManager texture_manager_;
  RenderbufferManager renderbuffer_manager_;
  scoped_ptr<MockErrorState> error_state_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint FramebufferInfoTest::kClient1Id;
const GLuint FramebufferInfoTest::kService1Id;
const GLint FramebufferInfoTest::kMaxTextureSize;
const GLint FramebufferInfoTest::kMaxCubemapSize;
const GLint FramebufferInfoTest::kMaxRenderbufferSize;
#endif

TEST_F(FramebufferInfoTest, Basic) {
  EXPECT_EQ(kService1Id, framebuffer_->service_id());
  EXPECT_FALSE(framebuffer_->IsDeleted());
  EXPECT_TRUE(NULL == framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(NULL == framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(NULL == framebuffer_->GetAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_TRUE(
      NULL == framebuffer_->GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(0), framebuffer_->GetColorAttachmentFormat());
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
}

TEST_F(FramebufferInfoTest, AttachRenderbuffer) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;
  const GLuint kRenderbufferClient3Id = 35;
  const GLuint kRenderbufferService3Id = 335;
  const GLuint kRenderbufferClient4Id = 36;
  const GLuint kRenderbufferService4Id = 336;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLenum kFormat1 = GL_RGBA4;
  const GLenum kBadFormat1 = GL_DEPTH_COMPONENT16;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 16;
  const GLsizei kHeight2 = 32;
  const GLenum kFormat2 = GL_DEPTH_COMPONENT16;
  const GLsizei kSamples2 = 0;
  const GLsizei kWidth3 = 16;
  const GLsizei kHeight3 = 32;
  const GLenum kFormat3 = GL_STENCIL_INDEX8;
  const GLsizei kSamples3 = 0;
  const GLsizei kWidth4 = 16;
  const GLsizei kHeight4 = 32;
  const GLenum kFormat4 = GL_STENCIL_INDEX8;
  const GLsizei kSamples4 = 0;

  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(
      framebuffer_->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT));

  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != NULL);

  // check adding one attachment
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, renderbuffer1);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA4),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_FALSE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Try a format that's not good for COLOR_ATTACHMENT0.
  renderbuffer_manager_.SetInfo(
      renderbuffer1, kSamples1, kBadFormat1, kWidth1, kHeight1);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());

  // Try a good format.
  renderbuffer_manager_.SetInfo(
      renderbuffer1, kSamples1, kFormat1, kWidth1, kHeight1);
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_FALSE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_FALSE(framebuffer_->IsCleared());

  // check adding another
  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient2Id, kRenderbufferService2Id);
  Renderbuffer* renderbuffer2 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient2Id);
  ASSERT_TRUE(renderbuffer2 != NULL);
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer2);
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());
  // The attachment has a size of 0,0 so depending on the order of the map
  // of attachments it could either get INCOMPLETE_ATTACHMENT because it's 0,0
  // or INCOMPLETE_DIMENSIONS because it's not the same size as the other
  // attachment.
  GLenum status = framebuffer_->IsPossiblyComplete();
  EXPECT_TRUE(
      status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ||
      status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT);
  EXPECT_FALSE(framebuffer_->IsCleared());

  renderbuffer_manager_.SetInfo(
      renderbuffer2, kSamples2, kFormat2, kWidth2, kHeight2);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));

  // check marking them as cleared.
  manager_.MarkAttachmentsAsCleared(
      framebuffer_, &renderbuffer_manager_, &texture_manager_);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Check adding one that is already cleared.
  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient3Id, kRenderbufferService3Id);
  Renderbuffer* renderbuffer3 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient3Id);
  ASSERT_TRUE(renderbuffer3 != NULL);
  renderbuffer_manager_.SetInfo(
      renderbuffer3, kSamples3, kFormat3, kWidth3, kHeight3);
  renderbuffer_manager_.SetCleared(renderbuffer3, true);

  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, renderbuffer3);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_TRUE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Check marking the renderbuffer as unclared.
  renderbuffer_manager_.SetInfo(
      renderbuffer1, kSamples1, kFormat1, kWidth1, kHeight1);
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_TRUE(framebuffer_->HasStencilAttachment());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_FALSE(framebuffer_->IsCleared());

  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth1, attachment->width());
  EXPECT_EQ(kHeight1, attachment->height());
  EXPECT_EQ(kSamples1, attachment->samples());
  EXPECT_EQ(kFormat1, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());

  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));

  // Clear it.
  manager_.MarkAttachmentsAsCleared(
      framebuffer_, &renderbuffer_manager_, &texture_manager_);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_TRUE(framebuffer_->IsCleared());

  // Check replacing an attachment
  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient4Id, kRenderbufferService4Id);
  Renderbuffer* renderbuffer4 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient4Id);
  ASSERT_TRUE(renderbuffer4 != NULL);
  renderbuffer_manager_.SetInfo(
      renderbuffer4, kSamples4, kFormat4, kWidth4, kHeight4);

  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, renderbuffer4);
  EXPECT_TRUE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->IsCleared());

  attachment = framebuffer_->GetAttachment(GL_STENCIL_ATTACHMENT);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth4, attachment->width());
  EXPECT_EQ(kHeight4, attachment->height());
  EXPECT_EQ(kSamples4, attachment->samples());
  EXPECT_EQ(kFormat4, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());

  // Check changing an attachment.
  renderbuffer_manager_.SetInfo(
      renderbuffer4, kSamples4, kFormat4, kWidth4 + 1, kHeight4);

  attachment = framebuffer_->GetAttachment(GL_STENCIL_ATTACHMENT);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth4 + 1, attachment->width());
  EXPECT_EQ(kHeight4, attachment->height());
  EXPECT_EQ(kSamples4, attachment->samples());
  EXPECT_EQ(kFormat4, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_FALSE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT),
            framebuffer_->IsPossiblyComplete());

  // Check removing it.
  framebuffer_->AttachRenderbuffer(GL_STENCIL_ATTACHMENT, NULL);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_TRUE(framebuffer_->HasDepthAttachment());
  EXPECT_FALSE(framebuffer_->HasStencilAttachment());

  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());

  // Remove depth, Set color to 0 size.
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, NULL);
  renderbuffer_manager_.SetInfo(renderbuffer1, kSamples1, kFormat1, 0, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());

  // Remove color.
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());
}

TEST_F(FramebufferInfoTest, AttachTexture) {
  const GLuint kTextureClient1Id = 33;
  const GLuint kTextureService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLint kDepth = 1;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLsizei kWidth1 = 16;
  const GLsizei kHeight1 = 32;
  const GLint kLevel1 = 0;
  const GLenum kFormat1 = GL_RGBA;
  const GLenum kBadFormat1 = GL_DEPTH_COMPONENT16;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLsizei kSamples1 = 0;
  const GLsizei kWidth2 = 16;
  const GLsizei kHeight2 = 32;
  const GLint kLevel2 = 0;
  const GLenum kFormat2 = GL_RGB;
  const GLenum kTarget2 = GL_TEXTURE_2D;
  const GLsizei kSamples2 = 0;
  const GLsizei kWidth3 = 75;
  const GLsizei kHeight3 = 123;
  const GLint kLevel3 = 0;
  const GLenum kFormat3 = GL_RGB565;
  const GLsizei kSamples3 = 0;
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
  EXPECT_FALSE(
      framebuffer_->HasUnclearedAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());

  texture_manager_.CreateTexture(kTextureClient1Id, kTextureService1Id);
  scoped_refptr<Texture> texture1(
      texture_manager_.GetTexture(kTextureClient1Id));
  ASSERT_TRUE(texture1 != NULL);

  // check adding one attachment
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture1, kTarget1, kLevel1);
  EXPECT_FALSE(framebuffer_->HasUnclearedAttachment(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(0), framebuffer_->GetColorAttachmentFormat());

  // Try format that doesn't work with COLOR_ATTACHMENT0
  texture_manager_.SetTarget(texture1, GL_TEXTURE_2D);
  texture_manager_.SetLevelInfo(
      texture1, GL_TEXTURE_2D, kLevel1,
      kBadFormat1, kWidth1, kHeight1, kDepth, kBorder, kBadFormat1, kType,
      true);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());

  // Try a good format.
  texture_manager_.SetLevelInfo(
      texture1, GL_TEXTURE_2D, kLevel1,
      kFormat1, kWidth1, kHeight1, kDepth, kBorder, kFormat1, kType, false);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_FALSE(framebuffer_->IsCleared());
  texture_manager_.SetLevelInfo(
      texture1, GL_TEXTURE_2D, kLevel1,
      kFormat1, kWidth1, kHeight1, kDepth, kBorder, kFormat1, kType, true);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat1),
            framebuffer_->GetColorAttachmentFormat());

  const Framebuffer::Attachment* attachment =
      framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth1, attachment->width());
  EXPECT_EQ(kHeight1, attachment->height());
  EXPECT_EQ(kSamples1, attachment->samples());
  EXPECT_EQ(kFormat1, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());

  // Check replacing an attachment
  texture_manager_.CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<Texture> texture2(
      texture_manager_.GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2 != NULL);
  texture_manager_.SetTarget(texture2, GL_TEXTURE_2D);
  texture_manager_.SetLevelInfo(
      texture2, GL_TEXTURE_2D, kLevel2,
      kFormat2, kWidth2, kHeight2, kDepth, kBorder, kFormat2, kType, true);

  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture2, kTarget2, kLevel2);
  EXPECT_EQ(static_cast<GLenum>(kFormat2),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());

  attachment = framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth2, attachment->width());
  EXPECT_EQ(kHeight2, attachment->height());
  EXPECT_EQ(kSamples2, attachment->samples());
  EXPECT_EQ(kFormat2, attachment->internal_format());
  EXPECT_TRUE(attachment->cleared());

  // Check changing attachment
  texture_manager_.SetLevelInfo(
      texture2, GL_TEXTURE_2D, kLevel3,
      kFormat3, kWidth3, kHeight3, kDepth, kBorder, kFormat3, kType, false);
  attachment = framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0);
  ASSERT_TRUE(attachment != NULL);
  EXPECT_EQ(kWidth3, attachment->width());
  EXPECT_EQ(kHeight3, attachment->height());
  EXPECT_EQ(kSamples3, attachment->samples());
  EXPECT_EQ(kFormat3, attachment->internal_format());
  EXPECT_FALSE(attachment->cleared());
  EXPECT_EQ(static_cast<GLenum>(kFormat3),
            framebuffer_->GetColorAttachmentFormat());
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            framebuffer_->IsPossiblyComplete());
  EXPECT_FALSE(framebuffer_->IsCleared());

  // Set to size 0
  texture_manager_.SetLevelInfo(
      texture2, GL_TEXTURE_2D, kLevel3,
      kFormat3, 0, 0, kDepth, kBorder, kFormat3, kType, false);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());

  // Check removing it.
  framebuffer_->AttachTexture(GL_COLOR_ATTACHMENT0, NULL, 0, 0);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) == NULL);
  EXPECT_EQ(static_cast<GLenum>(0), framebuffer_->GetColorAttachmentFormat());

  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            framebuffer_->IsPossiblyComplete());
  EXPECT_TRUE(framebuffer_->IsCleared());
}

TEST_F(FramebufferInfoTest, UnbindRenderbuffer) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kRenderbufferClient2Id = 34;
  const GLuint kRenderbufferService2Id = 334;

  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != NULL);
  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient2Id, kRenderbufferService2Id);
  Renderbuffer* renderbuffer2 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient2Id);
  ASSERT_TRUE(renderbuffer2 != NULL);

  // Attach to 2 attachment points.
  framebuffer_->AttachRenderbuffer(GL_COLOR_ATTACHMENT0, renderbuffer1);
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer1);
  // Check they were attached.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != NULL);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != NULL);
  // Unbind unattached renderbuffer.
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer2);
  // Should be no-op.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != NULL);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != NULL);
  // Unbind renderbuffer.
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer1);
  // Check they were detached
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) == NULL);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) == NULL);
}

TEST_F(FramebufferInfoTest, UnbindTexture) {
  const GLuint kTextureClient1Id = 33;
  const GLuint kTextureService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;

  texture_manager_.CreateTexture(kTextureClient1Id, kTextureService1Id);
  scoped_refptr<Texture> texture1(
      texture_manager_.GetTexture(kTextureClient1Id));
  ASSERT_TRUE(texture1 != NULL);
  texture_manager_.CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<Texture> texture2(
      texture_manager_.GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2 != NULL);

  // Attach to 2 attachment points.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture1, kTarget1, kLevel1);
  framebuffer_->AttachTexture(
      GL_DEPTH_ATTACHMENT, texture1, kTarget1, kLevel1);
  // Check they were attached.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != NULL);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != NULL);
  // Unbind unattached texture.
  framebuffer_->UnbindTexture(kTarget1, texture2);
  // Should be no-op.
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) != NULL);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) != NULL);
  // Unbind texture.
  framebuffer_->UnbindTexture(kTarget1, texture1);
  // Check they were detached
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_COLOR_ATTACHMENT0) == NULL);
  EXPECT_TRUE(framebuffer_->GetAttachment(GL_DEPTH_ATTACHMENT) == NULL);
}

TEST_F(FramebufferInfoTest, IsCompleteMarkAsComplete) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;

  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != NULL);
  texture_manager_.CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<Texture> texture2(
      texture_manager_.GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2 != NULL);

  // Check MarkAsComlete marks as complete.
  manager_.MarkAsComplete(framebuffer_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));

  // Check at attaching marks as not complete.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture2, kTarget1, kLevel1);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
  manager_.MarkAsComplete(framebuffer_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer1);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));

  // Check MarkAttachmentsAsCleared marks as complete.
  manager_.MarkAttachmentsAsCleared(
      framebuffer_, &renderbuffer_manager_, &texture_manager_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));

  // Check Unbind marks as not complete.
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer1);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
  manager_.MarkAsComplete(framebuffer_);
  EXPECT_TRUE(manager_.IsComplete(framebuffer_));
  framebuffer_->UnbindTexture(kTarget1, texture2);
  EXPECT_FALSE(manager_.IsComplete(framebuffer_));
}

TEST_F(FramebufferInfoTest, GetStatus) {
  const GLuint kRenderbufferClient1Id = 33;
  const GLuint kRenderbufferService1Id = 333;
  const GLuint kTextureClient2Id = 34;
  const GLuint kTextureService2Id = 334;
  const GLenum kTarget1 = GL_TEXTURE_2D;
  const GLint kLevel1 = 0;

  renderbuffer_manager_.CreateRenderbuffer(
      kRenderbufferClient1Id, kRenderbufferService1Id);
  Renderbuffer* renderbuffer1 =
      renderbuffer_manager_.GetRenderbuffer(kRenderbufferClient1Id);
  ASSERT_TRUE(renderbuffer1 != NULL);
  texture_manager_.CreateTexture(kTextureClient2Id, kTextureService2Id);
  scoped_refptr<Texture> texture2(
      texture_manager_.GetTexture(kTextureClient2Id));
  ASSERT_TRUE(texture2 != NULL);
  texture_manager_.SetTarget(texture2, GL_TEXTURE_2D);

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(&texture_manager_, GL_FRAMEBUFFER);

  // Check a second call for the same type does not call anything
  if (!framebuffer_->AllowFramebufferComboCompleteMapForTesting()) {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }
  framebuffer_->GetStatus(&texture_manager_, GL_FRAMEBUFFER);

  // Check changing the attachments calls CheckFramebufferStatus.
  framebuffer_->AttachTexture(
      GL_COLOR_ATTACHMENT0, texture2, kTarget1, kLevel1);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(&texture_manager_, GL_FRAMEBUFFER);

  // Check a second call for the same type does not call anything.
  if (!framebuffer_->AllowFramebufferComboCompleteMapForTesting()) {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }
  framebuffer_->GetStatus(&texture_manager_, GL_FRAMEBUFFER);

  // Check a second call with a different target calls CheckFramebufferStatus.
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check a second call for the same type does not call anything.
  if (!framebuffer_->AllowFramebufferComboCompleteMapForTesting()) {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check adding another attachment calls CheckFramebufferStatus.
  framebuffer_->AttachRenderbuffer(GL_DEPTH_ATTACHMENT, renderbuffer1);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check a second call for the same type does not call anything.
  if (!framebuffer_->AllowFramebufferComboCompleteMapForTesting()) {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check changing the format calls CheckFramebuffferStatus.
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), error_state_.get(), &texture_manager_,
      texture2, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check since it did not return FRAMEBUFFER_COMPLETE that it calls
  // CheckFramebufferStatus
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check putting it back does not call CheckFramebufferStatus.
  if (!framebuffer_->AllowFramebufferComboCompleteMapForTesting()) {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), error_state_.get(), &texture_manager_,
      texture2, GL_TEXTURE_WRAP_S, GL_REPEAT, GL_NO_ERROR);
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);

  // Check Unbinding does not call CheckFramebufferStatus
  framebuffer_->UnbindRenderbuffer(GL_RENDERBUFFER, renderbuffer1);
  if (!framebuffer_->AllowFramebufferComboCompleteMapForTesting()) {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }
  framebuffer_->GetStatus(&texture_manager_, GL_READ_FRAMEBUFFER);
}

}  // namespace gles2
}  // namespace gpu


