// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class TextureStorageTest : public testing::Test {
 protected:
  static const GLsizei kResolution = 64;
  void SetUp() override {
    GLManager::Options options;
    image_factory_.SetRequiredTextureType(GL_TEXTURE_2D);
    options.size = gfx::Size(kResolution, kResolution);
    options.image_factory = &image_factory_;
    gl_.Initialize(options);
    gl_.MakeCurrent();

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           tex_,
                           0);

    const GLubyte* extensions = glGetString(GL_EXTENSIONS);
    ext_texture_storage_available_ = strstr(
        reinterpret_cast<const char*>(extensions), "GL_EXT_texture_storage");
    chromium_texture_storage_image_available_ =
        strstr(reinterpret_cast<const char*>(extensions),
               "GL_CHROMIUM_texture_storage_image");
  }

  void TearDown() override { gl_.Destroy(); }

  TextureImageFactory image_factory_;
  GLManager gl_;
  GLuint tex_ = 0;
  GLuint fbo_ = 0;
  bool ext_texture_storage_available_ = false;
  bool chromium_texture_storage_image_available_ = false;
};

TEST_F(TextureStorageTest, CorrectPixels) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 2, GL_RGBA8_OES, 2, 2);

  // Mesa dirvers crash without rebinding to FBO. It's why
  // DISABLE_TEXTURE_STORAGE workaround is introduced. crbug.com/521904
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex_, 0);

  uint8_t source_pixels[16] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
  glTexSubImage2D(GL_TEXTURE_2D,
                  0,
                  0, 0,
                  2, 2,
                  GL_RGBA, GL_UNSIGNED_BYTE,
                  source_pixels);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 2, 2, 0, source_pixels, nullptr));
}

TEST_F(TextureStorageTest, IsImmutable) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);

  GLint param = 0;
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT_EXT, &param);
  EXPECT_TRUE(param);
}

TEST_F(TextureStorageTest, OneLevel) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);

  uint8_t source_pixels[64] = {0};

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, MultipleLevels) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 2, GL_RGBA8_OES, 2, 2);

  uint8_t source_pixels[16] = {0};

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 1, 1,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, 1, 1,
                  GL_RGBA, GL_UNSIGNED_BYTE, source_pixels);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, BadTarget) {
  if (!ext_texture_storage_available_)
    return;

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexStorage2DEXT(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 1, GL_RGBA8_OES, 4, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), glGetError());
}

TEST_F(TextureStorageTest, InvalidId) {
  if (!ext_texture_storage_available_)
    return;

  glDeleteTextures(1, &tex_);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, CannotRedefine) {
  if (!ext_texture_storage_available_)
    return;

  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 4, 4);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
}

TEST_F(TextureStorageTest, InternalFormatBleedingToTexImage) {
  if (!ext_texture_storage_available_)
    return;

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  // The context is ES2 context.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8_OES, 4, 4, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

TEST_F(TextureStorageTest, CorrectImagePixels) {
  if (!chromium_texture_storage_image_available_)
    return;

  glTexStorage2DImageCHROMIUM(GL_TEXTURE_2D, GL_RGBA8_OES, GL_SCANOUT_CHROMIUM,
                              2, 2);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex_, 0);

  uint8_t source_pixels[16] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                  source_pixels);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 2, 2, 0, source_pixels, nullptr));
}

}  // namespace gpu



