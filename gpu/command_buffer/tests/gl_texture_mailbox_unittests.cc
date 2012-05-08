// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gl/gl_share_group.h"

namespace gpu {

namespace {
uint32 ReadTexel(GLuint id, GLint x, GLint y) {
  GLint old_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         id,
                         0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  uint32 texel;
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &texel);

  glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);

  glDeleteFramebuffers(1, &fbo);

  return texel;
}
}

class GLTextureMailboxTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl1_.Initialize(gfx::Size(4, 4));
    gl2_.InitializeSharedMailbox(gfx::Size(4, 4), &gl1_);
  }

  virtual void TearDown() {
    gl1_.Destroy();
    gl2_.Destroy();
  }

  GLManager gl1_;
  GLManager gl2_;
};

TEST_F(GLTextureMailboxTest, ProduceAndConsumeTexture) {
  gl1_.MakeCurrent();

  GLbyte mailbox1[GL_MAILBOX_SIZE_CHROMIUM];
  glGenMailboxCHROMIUM(mailbox1);

  GLbyte mailbox2[GL_MAILBOX_SIZE_CHROMIUM];
  glGenMailboxCHROMIUM(mailbox2);

  GLuint tex1;
  glGenTextures(1, &tex1);

  glBindTexture(GL_TEXTURE_2D, tex1);
  uint32 source_pixel = 0xFF0000FF;
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               1, 1,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &source_pixel);

  glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox1);
  glFlush();

  gl2_.MakeCurrent();

  GLuint tex2;
  glGenTextures(1, &tex2);

  glBindTexture(GL_TEXTURE_2D, tex2);
  glConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox1);
  EXPECT_EQ(source_pixel, ReadTexel(tex2, 0, 0));
  glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox2);
  glFlush();

  gl1_.MakeCurrent();

  glBindTexture(GL_TEXTURE_2D, tex1);
  glConsumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox2);
  EXPECT_EQ(source_pixel, ReadTexel(tex1, 0, 0));
}

TEST_F(GLTextureMailboxTest, ProduceTextureValidatesKey) {
  GLuint tex;
  glGenTextures(1, &tex);

  glBindTexture(GL_TEXTURE_2D, tex);
  uint32 source_pixel = 0xFF0000FF;
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               1, 1,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &source_pixel);

  GLbyte invalid_mailbox[GL_MAILBOX_SIZE_CHROMIUM];
  glGenMailboxCHROMIUM(invalid_mailbox);
  ++invalid_mailbox[GL_MAILBOX_SIZE_CHROMIUM - 1];

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glProduceTextureCHROMIUM(GL_TEXTURE_2D, invalid_mailbox);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  // Ensure level 0 is still intact after glProduceTextureCHROMIUM fails.
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_EQ(source_pixel, ReadTexel(tex, 0, 0));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

TEST_F(GLTextureMailboxTest, ConsumeTextureValidatesKey) {
  GLuint tex;
  glGenTextures(1, &tex);

  glBindTexture(GL_TEXTURE_2D, tex);
  uint32 source_pixel = 0xFF0000FF;
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               1, 1,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               &source_pixel);

  GLbyte invalid_mailbox[GL_MAILBOX_SIZE_CHROMIUM];
  glGenMailboxCHROMIUM(invalid_mailbox);
  ++invalid_mailbox[GL_MAILBOX_SIZE_CHROMIUM - 1];

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glConsumeTextureCHROMIUM(GL_TEXTURE_2D, invalid_mailbox);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  // Ensure level 0 is still intact after glConsumeTextureCHROMIUM fails.
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  EXPECT_EQ(source_pixel, ReadTexel(tex, 0, 0));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}
}  // namespace gpu

