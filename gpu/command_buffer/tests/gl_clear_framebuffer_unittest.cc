// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stdint.h>

#include <vector>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

// A collection of tests that exercise the glClear workaround.
class GLClearFramebufferTest : public testing::TestWithParam<bool> {
 public:
  GLClearFramebufferTest() : color_handle_(0u), depth_handle_(0u) {}

 protected:
  void SetUp() override {
    if (GetParam()) {
      // Force the glClear() workaround so we can test it here.
      GpuDriverBugWorkarounds workarounds;
      workarounds.gl_clear_broken = true;
      gl_.InitializeWithWorkarounds(GLManager::Options(), workarounds);
      DCHECK(gl_.workarounds().gl_clear_broken);
    } else {
      gl_.Initialize(GLManager::Options());
      DCHECK(!gl_.workarounds().gl_clear_broken);
    }
  }

  bool IsApplicable() {
    // The workaround doesn't use VAOs which would cause a failure on a core
    // context and the hardware for each the workaround is necessary has a buggy
    // VAO implementation. So we skip testing the workaround on core profiles.
    return !GetParam() ||
           !gl_.context()->GetVersionInfo()->is_desktop_core_profile;
  }

  void InitDraw();
  void SetDrawColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
  void SetDrawDepth(GLfloat depth);
  void DrawQuad();

  void SetupFramebufferWithDepthStencil();
  void DestroyFramebuffer();

  void TearDown() override {
    GLTestHelper::CheckGLError("no errors", __LINE__);
    gl_.Destroy();
  }

 private:
  GLManager gl_;
  GLuint color_handle_;
  GLuint depth_handle_;

  GLuint framebuffer_handle_;
  GLuint color_buffer_handle_;
  GLuint depth_stencil_handle_;
};

void GLClearFramebufferTest::InitDraw() {
  static const char* v_shader_str =
      "attribute vec4 a_Position;\n"
      "uniform float u_depth;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = a_Position;\n"
      "   gl_Position.z = u_depth;\n"
      "}\n";
  static const char* f_shader_str =
      "precision mediump float;\n"
      "uniform vec4 u_draw_color;\n"
      "void main()\n"
      "{\n"
      "  gl_FragColor = u_draw_color;\n"
      "}\n";

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  DCHECK(program);
  glUseProgram(program);
  GLuint position_loc = glGetAttribLocation(program, "a_Position");

  GLTestHelper::SetupUnitQuad(position_loc);
  color_handle_ = glGetUniformLocation(program, "u_draw_color");
  DCHECK(color_handle_ != static_cast<GLuint>(-1));
  depth_handle_ = glGetUniformLocation(program, "u_depth");
  DCHECK(depth_handle_ != static_cast<GLuint>(-1));
}

void GLClearFramebufferTest::SetDrawColor(GLfloat r,
                                          GLfloat g,
                                          GLfloat b,
                                          GLfloat a) {
  glUniform4f(color_handle_, r, g, b, a);
}

void GLClearFramebufferTest::SetDrawDepth(GLfloat depth) {
  glUniform1f(depth_handle_, depth);
}

void GLClearFramebufferTest::DrawQuad() {
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void GLClearFramebufferTest::SetupFramebufferWithDepthStencil() {
  glGenFramebuffers(1, &framebuffer_handle_);
  DCHECK_NE(0u, framebuffer_handle_);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_handle_);

  static const int kFramebufferSize = 4;

  glGenRenderbuffers(1, &color_buffer_handle_);
  DCHECK_NE(0u, color_buffer_handle_);
  glBindRenderbuffer(GL_RENDERBUFFER, color_buffer_handle_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, kFramebufferSize,
                        kFramebufferSize);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, color_buffer_handle_);

  glGenRenderbuffers(1, &depth_stencil_handle_);
  DCHECK_NE(0u, depth_stencil_handle_);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_handle_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES,
                        kFramebufferSize, kFramebufferSize);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth_stencil_handle_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, depth_stencil_handle_);

  glViewport(0, 0, kFramebufferSize, kFramebufferSize);
}

void GLClearFramebufferTest::DestroyFramebuffer() {
  glDeleteRenderbuffers(1, &color_buffer_handle_);
  glDeleteRenderbuffers(1, &depth_stencil_handle_);
  glDeleteFramebuffers(1, &framebuffer_handle_);
}

INSTANTIATE_TEST_CASE_P(GLClearFramebufferTestWithParam,
                        GLClearFramebufferTest,
                        ::testing::Values(true, false));

TEST_P(GLClearFramebufferTest, ClearColor) {
  if (!IsApplicable()) {
    return;
  }

  glClearColor(1.0f, 0.5f, 0.25f, 0.5f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify.
  const uint8_t expected[] = {255, 128, 64, 128};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */, expected,
                                        nullptr));
}

TEST_P(GLClearFramebufferTest, ClearColorWithMask) {
  if (!IsApplicable()) {
    return;
  }

  glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify.
  const uint8_t expected[] = {255, 0, 0, 0};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, expected,
                                        nullptr));
}

// crbug.com/434094
#if !defined(OS_MACOSX)
TEST_P(GLClearFramebufferTest, ClearColorWithScissor) {
  if (!IsApplicable()) {
    return;
  }

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify.
  const uint8_t expected[] = {255, 255, 255, 255};
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, expected,
                                        nullptr));

  glScissor(0, 0, 0, 0);
  glEnable(GL_SCISSOR_TEST);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // Verify - no changes.
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, expected,
                                        nullptr));
}
#endif

TEST_P(GLClearFramebufferTest, ClearDepthStencil) {
  if (!IsApplicable()) {
    return;
  }

  SetupFramebufferWithDepthStencil();

  const GLuint kStencilRef = 1 << 2;
  InitDraw();
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();
  // Verify.
  const uint8_t kRed[] = {255, 0, 0, 255};
  const uint8_t kGreen[] = {0, 255, 0, 255};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kRed, nullptr));

  glClearStencil(kStencilRef);
  glClear(GL_STENCIL_BUFFER_BIT);
  glEnable(GL_STENCIL_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc(GL_NOTEQUAL, kStencilRef, 0xFFFFFFFF);

  SetDrawColor(0.0f, 1.0f, 0.0f, 1.0f);
  DrawQuad();
  // Verify - stencil should have failed, so still red.
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kRed, nullptr));

  glStencilFunc(GL_EQUAL, kStencilRef, 0xFFFFFFFF);
  DrawQuad();
  // Verify - stencil should have passed, so green.
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kGreen,
                                        nullptr));

  glEnable(GL_DEPTH_TEST);
  glClearDepthf(0.0f);
  glClear(GL_DEPTH_BUFFER_BIT);

  SetDrawDepth(0.5f);
  SetDrawColor(1.0f, 0.0f, 0.0f, 1.0f);
  DrawQuad();
  // Verify - depth test should have failed, so still green.
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kGreen,
                                        nullptr));

  glClearDepthf(0.9f);
  glClear(GL_DEPTH_BUFFER_BIT);
  DrawQuad();
  // Verify - depth test should have passed, so red.
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 0 /* tolerance */, kRed, nullptr));

  DestroyFramebuffer();
}

}  // namespace gpu
