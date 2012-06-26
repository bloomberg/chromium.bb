// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SHADER(Src) #Src

namespace gpu {

class BindUniformLocationTest : public testing::Test {
 protected:
  static const GLsizei kResolution = 4;
  virtual void SetUp() {
    gl_.Initialize(gfx::Size(kResolution, kResolution));
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  GLManager gl_;
};

TEST_F(BindUniformLocationTest, Basic) {
  ASSERT_TRUE(
      GLTestHelper::HasExtension("GL_CHROMIUM_bind_uniform_location"));

  static const char* v_shader_str = SHADER(
      attribute vec4 a_position;
      void main()
      {
         gl_Position = a_position;
      }
  );
  static const char* f_shader_str = SHADER(
      precision mediump float;
      uniform vec4 u_colorC;
      uniform vec4 u_colorB[2];
      uniform vec4 u_colorA;
      void main()
      {
        gl_FragColor = u_colorA + u_colorB[0] + u_colorB[1] + u_colorC;
      }
  );

  GLint color_a_location = 3;
  GLint color_b_location = 10;
  GLint color_c_location = 5;

  GLuint vertex_shader = GLTestHelper::LoadShader(
      GL_VERTEX_SHADER, v_shader_str);
  GLuint fragment_shader = GLTestHelper::LoadShader(
      GL_FRAGMENT_SHADER, f_shader_str);

  GLuint program = glCreateProgram();

  glBindUniformLocationCHROMIUM(program, color_a_location, "u_colorA");
  glBindUniformLocationCHROMIUM(program, color_b_location, "u_colorB[0]");
  glBindUniformLocationCHROMIUM(program, color_c_location, "u_colorC");

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Link the program
  glLinkProgram(program);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  EXPECT_EQ(1, linked);

  GLint position_loc = glGetAttribLocation(program, "a_position");

  GLTestHelper::SetupUnitQuad(position_loc);

  glUseProgram(program);

  static const float color_b[] = {
    0.0f, 0.50f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.75f, 0.0f,
  };
  glUniform4f(color_a_location, 0.25f, 0.0f, 0.0f, 0.0f);
  glUniform4fv(color_b_location, 2, color_b);
  glUniform4f(color_c_location, 0.0f, 0.0f, 0.0f, 1.0f);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  static const uint8 expected[] = { 64, 128, 192, 255 };
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, kResolution, kResolution, 1, expected));

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu



