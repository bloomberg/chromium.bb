// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
enum CopyType { TexImage, TexSubImage };
const CopyType kCopyTypes[] = {
    TexImage,
    TexSubImage,
};
}

// A collection of tests that exercise the GL_CHROMIUM_copy_texture extension.
class GLCopyTextureCHROMIUMTest
    : public testing::Test,
      public ::testing::WithParamInterface<CopyType> {
 protected:

  void CreateAndBindDestinationTextureAndFBO(GLenum target) {
    glGenTextures(2, textures_);
    glBindTexture(target, textures_[1]);

    // Some drivers (NVidia/SGX) require texture settings to be a certain way or
    // they won't report FRAMEBUFFER_COMPLETE.
    glTexParameterf(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &framebuffer_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                           textures_[1], 0);
  }

  void SetUp() override {
    gl_.Initialize(GLManager::Options());

    CreateAndBindDestinationTextureAndFBO(GL_TEXTURE_2D);
  }

  void TearDown() override {
    glDeleteTextures(2, textures_);
    glDeleteFramebuffers(1, &framebuffer_id_);
    gl_.Destroy();
  }

  void CreateBackingForTexture(GLenum target, GLsizei width, GLsizei height) {
    if (target == GL_TEXTURE_RECTANGLE_ARB) {
      GLuint image_id = glCreateGpuMemoryBufferImageCHROMIUM(
          width, height, GL_RGBA, GL_READ_WRITE_CHROMIUM);
      glBindTexImage2DCHROMIUM(target, image_id);
    } else {
      glTexImage2D(target, 0, GL_RGBA, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
    }
  }

  GLManager gl_;
  GLuint textures_[2];
  GLuint framebuffer_id_;
};

INSTANTIATE_TEST_CASE_P(CopyType,
                        GLCopyTextureCHROMIUMTest,
                        ::testing::ValuesIn(kCopyTypes));

// Test to ensure that the basic functionality of the extension works.
TEST_P(GLCopyTextureCHROMIUMTest, Basic) {
  CopyType copy_type = GetParam();
  uint8_t pixels[1 * 4] = {255u, 0u, 0u, 255u};

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  if (copy_type == TexImage) {
    glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
  } else {
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 0,
                             0, 1, 1, false, false, false);
  }
  EXPECT_TRUE(glGetError() == GL_NO_ERROR);

  // Check the FB is still bound.
  GLint value = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
  GLuint fb_id = value;
  EXPECT_EQ(framebuffer_id_, fb_id);

  // Check that FB is complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  GLTestHelper::CheckPixels(0, 0, 1, 1, 0, pixels);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_P(GLCopyTextureCHROMIUMTest, ImmutableTexture) {
  if (!GLTestHelper::HasExtension("GL_EXT_texture_storage")) {
    LOG(INFO) << "GL_EXT_texture_storage not supported. Skipping test...";
    return;
  }
  CopyType copy_type = GetParam();

  uint8_t pixels[1 * 4] = {255u, 0u, 0u, 255u};

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 1, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                  pixels);

  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8_OES, 1, 1);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         textures_[1], 0);
  EXPECT_TRUE(glGetError() == GL_NO_ERROR);

  if (copy_type == TexImage) {
    glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
    EXPECT_TRUE(glGetError() == GL_INVALID_OPERATION);
  } else {
    glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 0,
                             0, 1, 1, false, false, false);
    EXPECT_TRUE(glGetError() == GL_NO_ERROR);

    // Check the FB is still bound.
    GLint value = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
    GLuint fb_id = value;
    EXPECT_EQ(framebuffer_id_, fb_id);

    // Check that FB is complete.
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));

    GLTestHelper::CheckPixels(0, 0, 1, 1, 0, pixels);
    EXPECT_TRUE(GL_NO_ERROR == glGetError());
  }
}

TEST_P(GLCopyTextureCHROMIUMTest, InternalFormat) {
  CopyType copy_type = GetParam();
  GLint src_formats[] = {GL_ALPHA,     GL_RGB,             GL_RGBA,
                         GL_LUMINANCE, GL_LUMINANCE_ALPHA, GL_BGRA_EXT};
  GLint dest_formats[] = {GL_RGB, GL_RGBA, GL_BGRA_EXT};

  for (size_t src_index = 0; src_index < arraysize(src_formats); src_index++) {
    for (size_t dest_index = 0; dest_index < arraysize(dest_formats);
         dest_index++) {
      glBindTexture(GL_TEXTURE_2D, textures_[0]);
      glTexImage2D(GL_TEXTURE_2D, 0, src_formats[src_index], 1, 1, 0,
                   src_formats[src_index], GL_UNSIGNED_BYTE, nullptr);
      EXPECT_TRUE(GL_NO_ERROR == glGetError());

      if (copy_type == TexImage) {
        glCopyTextureCHROMIUM(textures_[0], textures_[1],
                              dest_formats[dest_index], GL_UNSIGNED_BYTE,
                              false, false, false);
      } else {
        glBindTexture(GL_TEXTURE_2D, textures_[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, dest_formats[dest_index], 1, 1, 0,
                     dest_formats[dest_index], GL_UNSIGNED_BYTE, nullptr);
        EXPECT_TRUE(GL_NO_ERROR == glGetError());

        glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0,
                                 0, 0, 0, 1, 1, false, false, false);
      }

      EXPECT_TRUE(GL_NO_ERROR == glGetError()) << "src_index:" << src_index
                                               << " dest_index:" << dest_index;
    }
  }
}

TEST_P(GLCopyTextureCHROMIUMTest, InternalFormatNotSupported) {
  CopyType copy_type = GetParam();
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // Check unsupported format reports error.
  GLint unsupported_dest_formats[] = {GL_ALPHA, GL_LUMINANCE,
                                      GL_LUMINANCE_ALPHA};
  for (size_t dest_index = 0; dest_index < arraysize(unsupported_dest_formats);
       dest_index++) {
    if (copy_type == TexImage) {
      glCopyTextureCHROMIUM(textures_[0], textures_[1],
                            unsupported_dest_formats[dest_index],
                            GL_UNSIGNED_BYTE, false, false, false);
    } else {
      glBindTexture(GL_TEXTURE_2D, textures_[1]);
      glTexImage2D(GL_TEXTURE_2D, 0, unsupported_dest_formats[dest_index], 1, 1,
                   0, unsupported_dest_formats[dest_index], GL_UNSIGNED_BYTE,
                   nullptr);
      glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0,
                               0, 0, 1, 1, false, false, false);
    }
    EXPECT_TRUE(GL_INVALID_OPERATION == glGetError())
        << "dest_index:" << dest_index;
  }
}

TEST_F(GLCopyTextureCHROMIUMTest, InternalFormatTypeCombinationNotSupported) {
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // Check unsupported internal_format/type combination reports error.
  struct FormatType { GLenum format, type; };
  FormatType unsupported_format_types[] = {
    {GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4},
    {GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1},
    {GL_RGBA, GL_UNSIGNED_SHORT_5_6_5},
  };
  for (size_t dest_index = 0; dest_index < arraysize(unsupported_format_types);
       dest_index++) {
    glCopyTextureCHROMIUM(
        textures_[0], textures_[1], unsupported_format_types[dest_index].format,
        unsupported_format_types[dest_index].type, false, false, false);
    EXPECT_TRUE(GL_INVALID_OPERATION == glGetError())
        << "dest_index:" << dest_index;
  }
}

// Test to ensure that the destination texture is redefined if the properties
// are different.
TEST_F(GLCopyTextureCHROMIUMTest, RedefineDestinationTexture) {
  uint8_t pixels[4 * 4] = {255u, 0u, 0u, 255u, 255u, 0u, 0u, 255u,
                           255u, 0u, 0u, 255u, 255u, 0u, 0u, 255u};

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_BGRA_EXT,
               1,
               1,
               0,
               GL_BGRA_EXT,
               GL_UNSIGNED_BYTE,
               pixels);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // GL_INVALID_OPERATION due to "intrinsic format" != "internal format".
  glTexSubImage2D(
      GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  EXPECT_TRUE(GL_INVALID_OPERATION == glGetError());
  // GL_INVALID_VALUE due to bad dimensions.
  glTexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  // If the dest texture has different properties, glCopyTextureCHROMIUM()
  // redefines them.
  glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                        GL_UNSIGNED_BYTE, false, false, false);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // glTexSubImage2D() succeeds because textures_[1] is redefined into 2x2
  // dimension and GL_RGBA format.
  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexSubImage2D(
      GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // Check the FB is still bound.
  GLint value = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
  GLuint fb_id = value;
  EXPECT_EQ(framebuffer_id_, fb_id);

  // Check that FB is complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  GLTestHelper::CheckPixels(1, 1, 1, 1, 0, &pixels[12]);
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
TEST_P(GLCopyTextureCHROMIUMTest, BasicStatePreservation) {
  CopyType copy_type = GetParam();
  uint8_t pixels[1 * 4] = {255u, 0u, 0u, 255u};

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  if (copy_type == TexSubImage) {
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
  }

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

    if (copy_type == TexImage) {
      glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                            GL_UNSIGNED_BYTE, false, false, false);
    } else {
      glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0,
                               0, 0, 1, 1, false, false, false);
    }
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

// Verify that invocation of the extension does not modify the bound
// texture state.
TEST_P(GLCopyTextureCHROMIUMTest, TextureStatePreserved) {
  CopyType copy_type = GetParam();
  // Setup the texture used for the extension invocation.
  uint8_t pixels[1 * 4] = {255u, 0u, 0u, 255u};
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  if (copy_type == TexSubImage) {
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
  }

  GLuint texture_ids[2];
  glGenTextures(2, texture_ids);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_ids[0]);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture_ids[1]);

  if (copy_type == TexImage) {
    glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
  } else {
    glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 0,
                             0, 1, 1, false, false, false);
  }
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  GLint active_texture = 0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
  EXPECT_EQ(GL_TEXTURE1, active_texture);

  GLint bound_texture = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture);
  EXPECT_EQ(texture_ids[1], static_cast<GLuint>(bound_texture));
  glBindTexture(GL_TEXTURE_2D, 0);

  bound_texture = 0;
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture);
  EXPECT_EQ(texture_ids[0], static_cast<GLuint>(bound_texture));
  glBindTexture(GL_TEXTURE_2D, 0);

  glDeleteTextures(2, texture_ids);

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

// Verify that invocation of the extension does not perturb the currently
// bound FBO state.
TEST_P(GLCopyTextureCHROMIUMTest, FBOStatePreserved) {
  CopyType copy_type = GetParam();
  // Setup the texture used for the extension invocation.
  uint8_t pixels[1 * 4] = {255u, 0u, 0u, 255u};
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);

  if (copy_type == TexSubImage) {
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
  }

  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0);

  GLuint renderbuffer_id;
  glGenRenderbuffers(1, &renderbuffer_id);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer_id);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 1, 1);

  GLuint framebuffer_id;
  glGenFramebuffers(1, &framebuffer_id);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture_id, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, renderbuffer_id);
  EXPECT_TRUE(
      GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER));

  // Test that we can write to the bound framebuffer
  uint8_t expected_color[4] = {255u, 255u, 0, 255u};
  glClearColor(1.0, 1.0, 0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected_color);

  if (copy_type == TexImage) {
    glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
  } else {
    glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 0,
                             0, 1, 1, false, false, false);
  }
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  EXPECT_TRUE(glIsFramebuffer(framebuffer_id));

  // Ensure that reading from the framebuffer produces correct pixels.
  GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected_color);

  uint8_t expected_color2[4] = {255u, 0, 255u, 255u};
  glClearColor(1.0, 0, 1.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected_color2);

  GLint bound_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
  EXPECT_EQ(framebuffer_id, static_cast<GLuint>(bound_fbo));

  GLint fbo_params = 0;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                        &fbo_params);
  EXPECT_EQ(GL_TEXTURE, fbo_params);

  fbo_params = 0;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                        &fbo_params);
  EXPECT_EQ(texture_id, static_cast<GLuint>(fbo_params));

  fbo_params = 0;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                        &fbo_params);
  EXPECT_EQ(GL_RENDERBUFFER, fbo_params);

  fbo_params = 0;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                        &fbo_params);
  EXPECT_EQ(renderbuffer_id, static_cast<GLuint>(fbo_params));

  glDeleteRenderbuffers(1, &renderbuffer_id);
  glDeleteTextures(1, &texture_id);
  glDeleteFramebuffers(1, &framebuffer_id);

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_P(GLCopyTextureCHROMIUMTest, ProgramStatePreservation) {
  CopyType copy_type = GetParam();
  // unbind the one created in setup.
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  GLManager gl2;
  GLManager::Options options;
  options.size = gfx::Size(16, 16);
  options.share_group_manager = &gl_;
  gl2.Initialize(options);
  gl_.MakeCurrent();

  static const char* v_shader_str =
      "attribute vec4 g_Position;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = g_Position;\n"
      "}\n";
  static const char* f_shader_str =
      "precision mediump float;\n"
      "void main()\n"
      "{\n"
      "  gl_FragColor = vec4(0,1,0,1);\n"
      "}\n";

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  glUseProgram(program);
  GLuint position_loc = glGetAttribLocation(program, "g_Position");
  glFlush();

  // Delete program from other context.
  gl2.MakeCurrent();
  glDeleteProgram(program);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());
  glFlush();

  // Program should still be usable on this context.
  gl_.MakeCurrent();

  GLTestHelper::SetupUnitQuad(position_loc);

  // test using program before
  uint8_t expected[] = {
      0, 255, 0, 255,
  };
  uint8_t zero[] = {
      0, 0, 0, 0,
  };
  glClear(GL_COLOR_BUFFER_BIT);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, zero));
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected));

  // Call copyTextureCHROMIUM
  uint8_t pixels[1 * 4] = {255u, 0u, 0u, 255u};
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels);
  if (copy_type == TexImage) {
    glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
  } else {
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 0,
                             0, 1, 1, false, false, false);
  }

  // test using program after
  glClear(GL_COLOR_BUFFER_BIT);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, zero));
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected));

  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  gl2.MakeCurrent();
  gl2.Destroy();
  gl_.MakeCurrent();
}

// Test that glCopyTextureCHROMIUM doesn't leak uninitialized textures.
TEST_P(GLCopyTextureCHROMIUMTest, UninitializedSource) {
  CopyType copy_type = GetParam();
  const GLsizei kWidth = 64, kHeight = 64;
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  if (copy_type == TexImage) {
    glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                          GL_UNSIGNED_BYTE, false, false, false);
  } else {
    glBindTexture(GL_TEXTURE_2D, textures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 0,
                             0, kWidth, kHeight, false, false, false);
  }
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  uint8_t pixels[kHeight][kWidth][4] = {{{1}}};
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int x = 0; x < kWidth; ++x) {
    for (int y = 0; y < kHeight; ++y) {
      EXPECT_EQ(0, pixels[y][x][0]);
      EXPECT_EQ(0, pixels[y][x][1]);
      EXPECT_EQ(0, pixels[y][x][2]);
      EXPECT_EQ(0, pixels[y][x][3]);
    }
  }

  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_F(GLCopyTextureCHROMIUMTest, CopySubTextureDimension) {
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 1, 1, 0,
                           0, 1, 1, false, false, false);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());

  // xoffset < 0
  glCopySubTextureCHROMIUM(textures_[0], textures_[1], -1, 1, 0,
                           0, 1, 1, false, false, false);
  EXPECT_TRUE(glGetError() == GL_INVALID_VALUE);

  // x < 0
  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 1, 1, -1,
                           0, 1, 1, false, false, false);
  EXPECT_TRUE(glGetError() == GL_INVALID_VALUE);

  // xoffset + width > dest_width
  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 2, 2, 0,
                           0, 2, 2, false, false, false);
  EXPECT_TRUE(glGetError() == GL_INVALID_VALUE);

  // x + width > source_width
  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 0, 1,
                           1, 2, 2, false, false, false);
  EXPECT_TRUE(glGetError() == GL_INVALID_VALUE);
}

TEST_F(GLCopyTextureCHROMIUMTest, CopyTextureInvalidTextureIds) {
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  glCopyTextureCHROMIUM(textures_[0], 99993, GL_RGBA,
                        GL_UNSIGNED_BYTE, false, false, false);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  glCopyTextureCHROMIUM(99994, textures_[1], GL_RGBA,
                        GL_UNSIGNED_BYTE, false, false, false);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  glCopyTextureCHROMIUM(99995, 99996, GL_RGBA, GL_UNSIGNED_BYTE,
                        false, false, false);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  glCopyTextureCHROMIUM(textures_[0], textures_[1], GL_RGBA,
                        GL_UNSIGNED_BYTE, false, false, false);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_F(GLCopyTextureCHROMIUMTest, CopySubTextureInvalidTextureIds) {
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);

  glCopySubTextureCHROMIUM(textures_[0], 99993, 1, 1, 0, 0, 1, 1,
                           false, false, false);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  glCopySubTextureCHROMIUM(99994, textures_[1], 1, 1, 0, 0, 1, 1,
                           false, false, false);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  glCopySubTextureCHROMIUM(99995, 99996, 1, 1, 0, 0, 1, 1, false,
                           false, false);
  EXPECT_TRUE(GL_INVALID_VALUE == glGetError());

  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 1, 1, 0,
                           0, 1, 1, false, false, false);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_F(GLCopyTextureCHROMIUMTest, CopySubTextureOffset) {
  uint8_t rgba_pixels[4 * 4] = {255u, 0u, 0u,   255u, 0u, 255u, 0u, 255u,
                                0u,   0u, 255u, 255u, 0u, 0u,   0u, 255u};
  glBindTexture(GL_TEXTURE_2D, textures_[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               rgba_pixels);

  uint8_t transparent_pixels[4 * 4] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                                       0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  glBindTexture(GL_TEXTURE_2D, textures_[1]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               transparent_pixels);

  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 1, 1, 0,
                           0, 1, 1, false, false, false);
  EXPECT_TRUE(glGetError() == GL_NO_ERROR);
  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 1, 0, 1,
                           0, 1, 1, false, false, false);
  EXPECT_TRUE(glGetError() == GL_NO_ERROR);
  glCopySubTextureCHROMIUM(textures_[0], textures_[1], 0, 1, 0,
                           1, 1, 1, false, false, false);
  EXPECT_TRUE(glGetError() == GL_NO_ERROR);

  // Check the FB is still bound.
  GLint value = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &value);
  GLuint fb_id = value;
  EXPECT_EQ(framebuffer_id_, fb_id);

  // Check that FB is complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  uint8_t transparent[1 * 4] = {0u, 0u, 0u, 0u};
  uint8_t red[1 * 4] = {255u, 0u, 0u, 255u};
  uint8_t green[1 * 4] = {0u, 255u, 0u, 255u};
  uint8_t blue[1 * 4] = {0u, 0u, 255u, 255u};
  GLTestHelper::CheckPixels(0, 0, 1, 1, 0, transparent);
  GLTestHelper::CheckPixels(1, 1, 1, 1, 0, red);
  GLTestHelper::CheckPixels(1, 0, 1, 1, 0, green);
  GLTestHelper::CheckPixels(0, 1, 1, 1, 0, blue);
  EXPECT_TRUE(GL_NO_ERROR == glGetError());
}

TEST_F(GLCopyTextureCHROMIUMTest, CopyTextureBetweenTexture2DAndRectangleArb) {
  if (!GLTestHelper::HasExtension("GL_ARB_texture_rectangle")) {
    LOG(INFO) <<
        "GL_ARB_texture_rectangle not supported. Skipping test...";
    return;
  }

  GLenum src_targets[] = {GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_2D};
  GLenum dest_targets[] = {GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_2D};
  GLsizei src_width = 30;
  GLsizei src_height = 14;
  GLsizei dest_width = 15;
  GLsizei dest_height = 13;
  GLsizei copy_region_x = 1;
  GLsizei copy_region_y = 1;
  GLsizei copy_region_width = 5;
  GLsizei copy_region_height = 3;
  uint8_t red[1 * 4] = {255u, 0u, 0u, 255u};
  uint8_t blue[1 * 4] = {0u, 0u, 255u, 255u};
  uint8_t green[1 * 4] = {0u, 255u, 0, 255u};
  uint8_t white[1 * 4] = {255u, 255u, 255u, 255u};
  uint8_t grey[1 * 4] = {199u, 199u, 199u, 255u};

  for (size_t src_index = 0; src_index < arraysize(src_targets); src_index++) {
    GLenum src_target = src_targets[src_index];
    for (size_t dest_index = 0; dest_index < arraysize(dest_targets);
         dest_index++) {
      GLenum dest_target = dest_targets[dest_index];

      // SetUp() sets up textures with the wrong parameters for this test, and
      // TearDown() expects to successfully delete textures/framebuffers, so
      // this is the right place for the delete/create calls.
      glDeleteTextures(2, textures_);
      glDeleteFramebuffers(1, &framebuffer_id_);
      CreateAndBindDestinationTextureAndFBO(dest_target);

      // Allocate source and destination textures.
      glBindTexture(src_target, textures_[0]);
      CreateBackingForTexture(src_target, src_width, src_height);

      glBindTexture(dest_target, textures_[1]);
      CreateBackingForTexture(dest_target, dest_width, dest_height);

      // The bottom left is red, bottom right is blue, top left is green, top
      // right is white.
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, src_target,
                             textures_[0], 0);
      glBindTexture(src_target, textures_[0]);
      for (GLint x = 0; x < src_width; ++x) {
        for (GLint y = 0; y < src_height; ++y) {
          uint8_t* data;
          if (x < src_width / 2) {
            data = y < src_height / 2 ? red : green;
          } else {
            data = y < src_height / 2 ? blue : white;
          }
          glTexSubImage2D(src_target, 0, x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                          data);
        }
      }

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, dest_target,
                             textures_[1], 0);
      glBindTexture(dest_target, textures_[1]);

      // Copy the subtexture x=[13,18) y=[6,9) to the destination.
      glClearColor(grey[0] / 255.f, grey[1] / 255.f, grey[2] / 255.f, 1.0);
      glClear(GL_COLOR_BUFFER_BIT);
      glCopySubTextureCHROMIUM(textures_[0], textures_[1], copy_region_x,
                               copy_region_y, 13, 6, copy_region_width,
                               copy_region_height, false, false, false);
      EXPECT_TRUE(GL_NO_ERROR == glGetError());

      for (GLint x = 0; x < dest_width; ++x) {
        for (GLint y = 0; y < dest_height; ++y) {
          if (x < copy_region_x || x >= copy_region_x + copy_region_width ||
              y < copy_region_y || y >= copy_region_y + copy_region_height) {
            GLTestHelper::CheckPixels(x, y, 1, 1, 0, grey);
            continue;
          }

          uint8_t* expected_color;
          if (x < copy_region_x + 2) {
            expected_color = y < copy_region_y + 1 ? red : green;
          } else {
            expected_color = y < copy_region_y + 1 ? blue : white;
          }
          GLTestHelper::CheckPixels(x, y, 1, 1, 0, expected_color);
        }
      }
    }
  }
}

}  // namespace gpu
