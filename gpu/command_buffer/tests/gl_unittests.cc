// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl_.Initialize(gfx::Size(4, 4));
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  GLManager gl_;
};

// Test that GL is at least minimally working.
TEST_F(GLTest, Basic) {
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  uint8 expected[] = { 0, 255, 0, 255, };
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected));
}

TEST_F(GLTest, SimpleShader) {
  static const char* v_shader_str =
      "attribute vec4 a_Position;\n"
      "void main()\n"
      "{\n"
      "   gl_Position = a_Position;\n"
      "}\n";
  static const char* f_shader_str =
      "precision mediump float;\n"
      "void main()\n"
      "{\n"
      "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
      "}\n";

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  glUseProgram(program);
  GLuint position_loc = glGetAttribLocation(program, "a_Position");

  GLTestHelper::SetupUnitQuad(position_loc);

  uint8 expected_clear[] = { 127, 0, 255, 0, };
  glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 1, expected_clear));
  uint8 expected_draw[] = { 0, 255, 0, 255, };
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, 1, 1, 0, expected_draw));
}

TEST_F(GLTest, GetString) {
  EXPECT_STREQ(
      "OpenGL ES 2.0 Chromium",
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  EXPECT_STREQ(
      "OpenGL ES GLSL ES 1.0 Chromium",
      reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
  EXPECT_STREQ(
      "Chromium",
      reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
  EXPECT_STREQ(
      "Chromium",
      reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
}

}  // namespace gpu

