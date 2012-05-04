// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

// A collection of tests that exercise the GL_CHROMIUM_copy_texture extension.
class GLCopyTextureCHROMIUMTest : public testing::Test {
 protected:
  GLCopyTextureCHROMIUMTest() : gl_(NULL, NULL) {
  }

  virtual void SetUp() {
    gl_.Initialize(gfx::Size(4, 4));

    glGenTextures(2, textures_);
    glBindTexture(GL_TEXTURE_2D, textures_[1]);

    glGenFramebuffers(1, &framebuffer_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           textures_[1], 0);
  }

  virtual void TearDown() {
    glDeleteTextures(2, textures_);
    glDeleteFramebuffers(1, &framebuffer_id_);
    gl_.Destroy();
  }

  GLManager gl_;
  GLuint textures_[2];
  GLuint framebuffer_id_;
};

// Test to ensure that the basic functionality of the extension works.
TEST_F(GLCopyTextureCHROMIUMTest, Basic) {
  uint8 pixels[1 * 4] = { 255u, 0u, 0u, 255u };

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  glCopyTextureCHROMIUM(GL_TEXTURE_2D, textures_[0], textures_[1], 0, GL_RGBA);
  EXPECT_TRUE(glGetError() == GL_NO_ERROR);

  uint8 copied_pixels[1 * 4];
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, copied_pixels);
  EXPECT_EQ(pixels[0], copied_pixels[0]);
  EXPECT_EQ(pixels[1], copied_pixels[1]);
  EXPECT_EQ(pixels[2], copied_pixels[2]);
  EXPECT_EQ(pixels[3], copied_pixels[3]);

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

// Test that the extension respects the flip-y pixel storage setting.
TEST_F(GLCopyTextureCHROMIUMTest, FlipY) {
  uint8 pixels[2][2][4];
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      pixels[y][x][0] = x + y;
      pixels[y][x][1] = x + y;
      pixels[y][x][2] = x + y;
      pixels[y][x][3] = 255u;
    }
  }

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  glPixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  glCopyTextureCHROMIUM(GL_TEXTURE_2D, textures_[0], textures_[1], 0, GL_RGBA);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  uint8 copied_pixels[2][2][4];
  glReadPixels(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, copied_pixels);
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y)
      EXPECT_EQ(pixels[1-y][x][0], copied_pixels[y][x][0]);
  }

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

// Test that the extension respects the GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM
// storage setting.
TEST_F(GLCopyTextureCHROMIUMTest, PremultiplyAlpha) {
  uint8 pixels[1 * 4] = { 2, 2, 2, 128 };

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  glPixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM, GL_TRUE);
  glCopyTextureCHROMIUM(GL_TEXTURE_2D, textures_[0], textures_[1], 0, GL_RGBA);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  uint8 copied_pixels[1 * 4];
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, copied_pixels);
  EXPECT_EQ(1u, copied_pixels[0]);
  EXPECT_EQ(1u, copied_pixels[1]);
  EXPECT_EQ(1u, copied_pixels[2]);
  EXPECT_EQ(128u, copied_pixels[3]);

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_F(GLCopyTextureCHROMIUMTest, FlipYAndPremultiplyAlpha) {
  uint8 pixels[2][2][4];
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      pixels[y][x][0] = x + y;
      pixels[y][x][1] = x + y;
      pixels[y][x][2] = x + y;
      pixels[y][x][3] = 128u;
    }
  }

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  glPixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  glPixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM, GL_TRUE);
  glCopyTextureCHROMIUM(GL_TEXTURE_2D, textures_[0], textures_[1], 0, GL_RGBA);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  uint8 copied_pixels[2][2][4];
  glReadPixels(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, copied_pixels);
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      EXPECT_EQ(pixels[1-y][x][0] / 2, copied_pixels[y][x][0]);
      EXPECT_EQ(pixels[1-y][x][1] / 2, copied_pixels[y][x][1]);
      EXPECT_EQ(pixels[1-y][x][2] / 2, copied_pixels[y][x][2]);
      EXPECT_EQ(pixels[1-y][x][3], copied_pixels[y][x][3]);
    }
  }

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

namespace {

void glEnableDisable(GLint param, GLboolean value) {
  if (value)
    glEnable(param);
  else
    glDisable(param);
}

}  // unnamed namespace

// Validate that some basic GL state is not touched upon execution of
// the extension.
TEST_F(GLCopyTextureCHROMIUMTest, BasicStatePreservation) {
  uint8 pixels[1 * 4] = { 255u, 0u, 0u, 255u };

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  GLboolean reference_settings[2] = { GL_TRUE, GL_FALSE };
  for (int x = 0; x < 2; ++x) {
    GLboolean setting = reference_settings[x];
    glEnableDisable(GL_DEPTH_TEST, setting);
    glEnableDisable(GL_SCISSOR_TEST, setting);
    glEnableDisable(GL_STENCIL_TEST, setting);
    glEnableDisable(GL_CULL_FACE, setting);
    glEnableDisable(GL_BLEND, setting);
    glColorMask(setting, setting, setting, setting);
    glDepthMask(setting);

    glActiveTexture(GL_TEXTURE1 + x);

    glCopyTextureCHROMIUM(GL_TEXTURE_2D, textures_[0], textures_[1], 0,
                          GL_RGBA);
    EXPECT_TRUE(GL_NO_ERROR == glGetError());

    EXPECT_EQ(setting, glIsEnabled(GL_DEPTH_TEST));
    EXPECT_EQ(setting, glIsEnabled(GL_SCISSOR_TEST));
    EXPECT_EQ(setting, glIsEnabled(GL_STENCIL_TEST));
    EXPECT_EQ(setting, glIsEnabled(GL_CULL_FACE));
    EXPECT_EQ(setting, glIsEnabled(GL_BLEND));

    GLboolean bool_array[4] = { GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE };
    glGetBooleanv(GL_DEPTH_WRITEMASK, bool_array);
    EXPECT_EQ(setting, bool_array[0]);

    bool_array[0] = GL_FALSE;
    glGetBooleanv(GL_COLOR_WRITEMASK, bool_array);
    EXPECT_EQ(setting, bool_array[0]);
    EXPECT_EQ(setting, bool_array[1]);
    EXPECT_EQ(setting, bool_array[2]);
    EXPECT_EQ(setting, bool_array[3]);

    GLint active_texture = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
    EXPECT_EQ(GL_TEXTURE1 + x, active_texture);
  }

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
};

}  // namespace gpu
