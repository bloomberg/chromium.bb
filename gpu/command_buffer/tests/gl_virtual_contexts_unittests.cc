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

class GLVirtualContextsTest : public testing::Test {
 protected:
  static const int kSize0 = 4;
  static const int kSize1 = 8;
  static const int kSize2 = 16;

  virtual void SetUp() {
    GLManager::Options options;
    options.size = gfx::Size(kSize0, kSize0);
    gl_real_.Initialize(options);
    gl_real_shared_.Initialize(options);
    options.virtual_manager = &gl_real_shared_;
    options.size = gfx::Size(kSize1, kSize1);
    gl1_.Initialize(options);
    options.size = gfx::Size(kSize2, kSize2);
    gl2_.Initialize(options);
  }

  virtual void TearDown() {
    gl1_.Destroy();
    gl2_.Destroy();
    gl_real_shared_.Destroy();
    gl_real_.Destroy();
  }

  GLManager gl_real_;
  GLManager gl_real_shared_;
  GLManager gl1_;
  GLManager gl2_;
};

namespace {

void SetupSimpleShader(const uint8* color) {
  static const char* v_shader_str = SHADER(
      attribute vec4 a_Position;
      void main()
      {
         gl_Position = a_Position;
      }
   );

  static const char* f_shader_str = SHADER(
      precision mediump float;
      uniform vec4 u_color;
      void main()
      {
        gl_FragColor = u_color;
      }
  );

  GLuint program = GLTestHelper::LoadProgram(v_shader_str, f_shader_str);
  glUseProgram(program);

  GLuint position_loc = glGetAttribLocation(program, "a_Position");

  GLTestHelper::SetupUnitQuad(position_loc);

  GLuint color_loc = glGetUniformLocation(program, "u_color");
  glUniform4f(
      color_loc,
      color[0] / 255.0f,
      color[1] / 255.0f,
      color[2] / 255.0f,
      color[3] / 255.0f);
}

void TestDraw(int size) {
  uint8 expected_clear[] = { 127, 0, 255, 0, };
  glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, size, size, 1, expected_clear));
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

}  // anonymous namespace

TEST_F(GLVirtualContextsTest, Basic) {
  struct TestInfo {
    int size;
    uint8 color[4];
    GLManager* manager;
  };
  const int kNumTests = 3;
  TestInfo tests[] = {
    { kSize0, { 255, 0, 0, 0, }, &gl_real_, },
    { kSize1, { 0, 255, 0, 0, }, &gl1_, },
    { kSize2, { 0, 0, 255, 0, }, &gl2_, },
  };

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    SetupSimpleShader(test.color);
  }

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    TestDraw(test.size);
  }

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    EXPECT_TRUE(GLTestHelper::CheckPixels(
        0, 0, test.size, test.size, 0, test.color));
  }

  for (int ii = 0; ii < kNumTests; ++ii) {
    const TestInfo& test = tests[ii];
    GLManager* gl_manager = test.manager;
    gl_manager->MakeCurrent();
    GLTestHelper::CheckGLError("no errors", __LINE__);
  }
}

}  // namespace gpu

