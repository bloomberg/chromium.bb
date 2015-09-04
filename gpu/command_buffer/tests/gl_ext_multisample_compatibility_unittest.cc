// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER0(Src) #Src

namespace {
uint32 GetPixel(GLint x,
                GLint y,
                uint8* r = nullptr,
                uint8* g = nullptr,
                uint8* b = nullptr,
                uint8* a = nullptr) {
  uint8 pixels[4];
  memset(pixels, GLTestHelper::kCheckClearValue, sizeof(pixels));
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  if (r) {
    *r = pixels[0];
  }
  if (g) {
    *g = pixels[1];
  }
  if (b) {
    *b = pixels[2];
  }
  if (a) {
    *a = pixels[3];
  }
  uint32 result;
  memcpy(&result, pixels, sizeof(result));
  return result;
}
}  // namespace

namespace gpu {

class EXTMultisampleCompatibilityTest : public testing::Test {
 public:
 protected:
  static const GLuint kWidth = 100;
  static const GLuint kHeight = 100;

  void SetUp() override { gl_.Initialize(GLManager::Options()); }

  void TearDown() override { gl_.Destroy(); }

  void PrepareForDraw() {
    static const char* v_shader_str = SHADER0(
        attribute vec4 a_Position; void main() { gl_Position = a_Position; });
    static const char* f_shader_str =
        SHADER0(precision mediump float; uniform vec4 color;
                void main() { gl_FragColor = color; });

    GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
    glUseProgram(program);
    GLuint position_loc = glGetAttribLocation(program, "a_Position");
    color_loc_ = glGetUniformLocation(program, "color");

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    static float vertices[] = {
        1.0f,  1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(position_loc);
    glVertexAttribPointer(position_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);

    // Create a sample buffer.
    GLsizei num_samples = 4, max_samples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    num_samples = std::min(num_samples, max_samples);
    GLuint sample_rb;
    glGenRenderbuffers(1, &sample_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, sample_rb);
    glRenderbufferStorageMultisampleCHROMIUM(GL_RENDERBUFFER, num_samples,
                                             GL_RGBA8_OES, kWidth, kHeight);
    GLint param = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES,
                                 &param);
    EXPECT_GE(param, num_samples);

    glGenFramebuffers(1, &sample_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, sample_fbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, sample_rb);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create another FBO to resolve the multisample buffer into.
    GLuint resolve_tex;
    glGenTextures(1, &resolve_tex);
    glBindTexture(GL_TEXTURE_2D, resolve_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &resolve_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolve_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           resolve_tex, 0);
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glViewport(0, 0, kWidth, kHeight);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, sample_fbo_);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void PrepareForVerify() {
    // Resolve.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sample_fbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo_);
    glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebufferCHROMIUM(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve_fbo_);
  }

  bool IsApplicable() const {
    return GLTestHelper::HasExtension("GL_EXT_multisample_compatibility") &&
           GLTestHelper::HasExtension("GL_CHROMIUM_framebuffer_multisample") &&
           GLTestHelper::HasExtension("GL_OES_rgb8_rgba8");
  }
  GLuint sample_fbo_;
  GLuint resolve_fbo_;
  GLuint color_loc_;

  GLManager gl_;
};

TEST_F(EXTMultisampleCompatibilityTest, TestSimple) {
  if (!IsApplicable()) {
    return;
  }

  EXPECT_TRUE(glIsEnabled(GL_MULTISAMPLE_EXT));
  glDisable(GL_MULTISAMPLE_EXT);
  EXPECT_FALSE(glIsEnabled(GL_MULTISAMPLE_EXT));
  glEnable(GL_MULTISAMPLE_EXT);
  EXPECT_TRUE(glIsEnabled(GL_MULTISAMPLE_EXT));

  EXPECT_FALSE(glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE_EXT));
  glEnable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
  EXPECT_TRUE(glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE_EXT));
  glDisable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
  EXPECT_FALSE(glIsEnabled(GL_SAMPLE_ALPHA_TO_ONE_EXT));

  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

TEST_F(EXTMultisampleCompatibilityTest, DrawAndResolve) {
  // Test that disabling GL_MULTISAMPLE_EXT is handled correctly.
  if (!IsApplicable()) {
    return;
  }
  PrepareForDraw();

  static const float kBlue[] = {0.0f, 0.0f, 1.0f, 1.0f};
  static const float kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};
  static const float kRed[] = {1.0f, 0.0f, 0.0f, 1.0f};

  // Green: from top right to bottom left.
  glUniform4fv(color_loc_, 1, kGreen);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Blue: from top left to bottom right.
  glUniform4fv(color_loc_, 1, kBlue);
  glDrawArrays(GL_TRIANGLES, 3, 3);

  // Red, without MSAA: from bottom left to top right.
  glDisable(GL_MULTISAMPLE_EXT);
  glUniform4fv(color_loc_, 1, kRed);
  glDrawArrays(GL_TRIANGLES, 6, 3);

  PrepareForVerify();

  // Top-left: Green triangle hits 50% of the samples. Blue hits 50% of the
  // samples. The pixel should be {0, 127, 127, 255}. For accuracy reasons,
  // Test only that multisampling is on, eg. that fractional results result
  // in <255 pixel value.
  uint8 r, g, b, a;
  GetPixel(0, kHeight - 1, &r, &g, &b, &a);
  EXPECT_EQ(0u, r);
  EXPECT_GT(255u, g);
  EXPECT_GT(255u, b);
  EXPECT_EQ(255u, a);

  // Top-right: Green triangle hits 50% of the samples. Red hits 100% of the
  // samples, since it is not multisampled. Should be {255, 0, 0, 255}.
  GetPixel(kWidth - 1, kHeight - 1, &r, &g, &b, &a);
  EXPECT_EQ(255u, r);
  EXPECT_EQ(0u, g);
  EXPECT_EQ(0u, b);
  EXPECT_EQ(255u, a);

  // Bottom-right: Blue triangle hits 50% of the samples. Red triangle
  // hits 100% of the samples. Should be {255, 0, 0, 255}.
  GetPixel(kWidth - 1, 0, &r, &g, &b, &a);
  EXPECT_EQ(255u, r);
  EXPECT_EQ(0u, g);
  EXPECT_EQ(0u, b);
  EXPECT_EQ(255u, a);

  // Low-left: Green triangle hits 50% of the samples. Blue hits 100% of the
  // samples. Red hits 100% of the samples, since it is not multisampled.
  // Should be {255, 0, 0, 255}.
  GetPixel(0, 0, &r, &g, &b, &a);
  EXPECT_EQ(255u, r);
  EXPECT_EQ(0u, g);
  EXPECT_EQ(0u, b);
  EXPECT_EQ(255u, a);

  // Middle point: Green triangle hits 50% of the samples, blue 50% of the
  // samples. Red does not hit.
  GetPixel(kWidth / 2 - 1, kHeight / 2, &r, &g, &b, &a);
  EXPECT_EQ(0u, r);
  EXPECT_GT(255u, g);
  EXPECT_GT(255u, b);
  EXPECT_EQ(255u, a);
}

TEST_F(EXTMultisampleCompatibilityTest, DrawAlphaOneAndResolve) {
  // Test that enabling GL_SAMPLE_ALPHA_TO_ONE_EXT affects rendering.
  if (!IsApplicable()) {
    return;
  }

  // SAMPLE_ALPHA_TO_ONE is specified to transform alpha values of
  // covered samples to 1.0. In order to detect it, we use non-1.0
  // alpha.
  static const float kBlue[] = {0.0f, 0.0f, 1.0f, 0.5f};
  static const float kGreen[] = {0.0f, 1.0f, 0.0f, 0.5f};
  static const float kRed[] = {1.0f, 0.0f, 0.0f, 0.5f};

  // Different drivers seem to behave differently with respect to
  // resulting alpha value. These might be due to different MSAA
  // sample counts causing different samples to hit.  Testing exact or
  // even approximate sample values is not that easy. Thus, just test
  // representative positions which have fractional pixels, inspecting
  // that normal rendering is different to SAMPLE_ALPHA_TO_ONE
  // rendering.
  uint32 result[2][5];

  for (int pass = 0; pass < 2; ++pass) {
    PrepareForDraw();
    if (pass == 1) {
      glEnable(GL_SAMPLE_ALPHA_TO_ONE_EXT);
    }
    glUniform4fv(color_loc_, 1, kGreen);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glUniform4fv(color_loc_, 1, kBlue);
    glDrawArrays(GL_TRIANGLES, 3, 3);

    glDisable(GL_MULTISAMPLE_EXT);
    glUniform4fv(color_loc_, 1, kRed);
    glDrawArrays(GL_TRIANGLES, 6, 3);

    PrepareForVerify();

    // Top-left.
    result[pass][0] = GetPixel(0, kHeight - 1);
    // Top-right.
    result[pass][1] = GetPixel(kWidth - 1, kHeight - 1);
    // Bottom-right.
    result[pass][2] = GetPixel(kWidth - 1, 0);
    // Low-left.
    result[pass][3] = GetPixel(1, 1);
    // Middle point.
    result[pass][4] = GetPixel(kWidth / 2 - 1, kHeight / 2);
  }
  EXPECT_NE(result[0][0], result[1][0]);
  EXPECT_NE(result[0][1], result[1][1]);
  EXPECT_NE(result[0][2], result[1][2]);
  EXPECT_NE(result[0][3], result[1][3]);
  EXPECT_NE(result[0][4], result[1][4]);
}

}  // namespace gpu
