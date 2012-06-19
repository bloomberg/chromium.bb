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

class ConsistenUniformLocationsTest : public testing::Test {
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

namespace {

struct FormatType {
  GLenum format;
  GLenum type;
};

}  // anonymous namespace

TEST_F(ConsistenUniformLocationsTest, Basic) {
  ASSERT_TRUE(
      GLTestHelper::HasExtension("GL_CHROMIUM_consistent_uniform_locations"));

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

  static const GLUniformDefinitionCHROMIUM defs[] = {
    { GL_FLOAT_VEC4, 1, "u_colorC", },
    { GL_FLOAT_VEC4, 2, "u_colorB", },
    { GL_FLOAT_VEC4, 1, "u_colorA", },
  };

  GLint locations[4];

  glGetUniformLocationsCHROMIUM(
      defs, arraysize(defs), arraysize(locations), locations);

  GLint u_colorCLocation = locations[0];
  GLint u_colorB0Location = locations[1];
  GLint u_colorB1Location = locations[2];
  GLint u_colorALocation = locations[3];

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);

  GLint position_loc = glGetAttribLocation(program, "a_position");

  GLTestHelper::SetupUnitQuad(position_loc);

  glUseProgram(program);

  glUniform4f(u_colorALocation, 0.25f, 0.0f, 0.0f, 0.0f);
  glUniform4f(u_colorB0Location, 0.0f, 0.50f, 0.0f, 0.0f);
  glUniform4f(u_colorB1Location, 0.0f, 0.0f, 0.75f, 0.0f);
  glUniform4f(u_colorCLocation, 0.0f, 0.0f, 0.0f, 1.0f);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  static const uint8 expected[] = { 64, 128, 192, 255 };
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, kResolution, kResolution, 1, expected));

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu



