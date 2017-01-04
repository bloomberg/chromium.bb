// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/command_line.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

#define SHADER(Src) #Src

namespace gpu {

class ES3MapBufferRangeTest : public testing::Test {
 protected:
  static const GLsizei kCanvasSize = 4;

  void SetUp() override {
    GLManager::Options options;
    options.size = gfx::Size(kCanvasSize, kCanvasSize);
    options.context_type = gles2::CONTEXT_TYPE_OPENGLES3;

    base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
    gl_.InitializeWithCommandLine(options, cmd_line);
  }

  bool ShouldSkipTest() const {
    // If a driver isn't capable of supporting ES3 context, creating
    // ContextGroup will fail.
    // See crbug.com/654709.
    return (!gl_.decoder() || !gl_.decoder()->GetContextGroup());
  }

  void TearDown() override { gl_.Destroy(); }

  GLuint SetupSimpleProgram(GLint* ret_position_loc) {
    static const char* v_shader_src =
        SHADER(attribute vec2 a_position;
               void main() { gl_Position = vec4(a_position, 0.0, 1.0); });
    static const char* f_shader_src =
        SHADER(precision mediump float;
               void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); });
    GLuint program = GLTestHelper::LoadProgram(v_shader_src, f_shader_src);
    EXPECT_LT(0u, program);
    glUseProgram(program);
    *ret_position_loc = glGetAttribLocation(program, "a_position");
    EXPECT_LE(0, *ret_position_loc);
    return program;
  }

  GLuint SetupTransformFeedbackProgram(GLint* ret_position_loc) {
    static const char* v_shader_src =
        "#version 300 es\n"
        "in vec2 a_position;\n"
        "out vec3 var0;\n"
        "out vec2 var1;\n"
        "void main() {\n"
        "  var0 = vec3(a_position, 0.5);\n"
        "  var1 = a_position;\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "}";
    static const char* f_shader_src =
        "#version 300 es\n"
        "precision mediump float;\n"
        "out vec4 color;\n"
        "in vec3 var0;\n"
        "in vec2 var1;\n"
        "void main() {\n"
        "  color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}";
    static const char* varyings[] = {"var0", "var1"};
    GLuint program = GLTestHelper::LoadProgram(v_shader_src, f_shader_src);
    EXPECT_LT(0u, program);
    glTransformFeedbackVaryings(program, 2, varyings, GL_SEPARATE_ATTRIBS);
    glLinkProgram(program);
    GLint link_status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    EXPECT_EQ(GL_TRUE, link_status);
    glUseProgram(program);
    *ret_position_loc = glGetAttribLocation(program, "a_position");
    EXPECT_LE(0, *ret_position_loc);
    return program;
  }

  GLManager gl_;
};

TEST_F(ES3MapBufferRangeTest, DrawArraysAndInstanced) {
  if (ShouldSkipTest())
    return;

  const uint8_t kRedColor[] = {255, 0, 0, 255};
  const uint8_t kBlackColor[] = {0, 0, 0, 255};
  const GLsizei kPrimCount = 4;

  GLint position_loc = 0;
  SetupSimpleProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint buffer = GLTestHelper::SetupUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_LT(0u, buffer);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glMapBufferRange(GL_ARRAY_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  // The following test is necessary to make sure draw calls do not just check
  // bound buffers, but actual buffers that are attached to the enabled vertex
  // attribs.
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glUnmapBuffer(GL_ARRAY_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArraysInstancedANGLE(GL_TRIANGLES, 0, 6, kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));
}

TEST_F(ES3MapBufferRangeTest, DrawElementsAndInstanced) {
  if (ShouldSkipTest())
    return;

  const uint8_t kRedColor[] = {255, 0, 0, 255};
  const uint8_t kBlackColor[] = {0, 0, 0, 255};
  const GLsizei kPrimCount = 4;

  GLint position_loc = 0;
  SetupSimpleProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  std::vector<GLuint> buffers =
      GLTestHelper::SetupIndexedUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_EQ(2u, buffers.size());
  EXPECT_LT(0u, buffers[0]);
  EXPECT_LT(0u, buffers[1]);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glMapBufferRange(GL_ARRAY_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  // The following test is necessary to make sure draw calls do not just check
  // bound buffers, but actual buffers that are attached to the enabled vertex
  // attribs.
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);

  glUnmapBuffer(GL_ARRAY_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kBlackColor, nullptr));

  glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElementsInstancedANGLE(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0,
                               kPrimCount);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_TRUE(GLTestHelper::CheckPixels(0, 0, kCanvasSize, kCanvasSize, 1,
                                        kRedColor, nullptr));
}

TEST_F(ES3MapBufferRangeTest, ReadPixels) {
  if (ShouldSkipTest())
    return;

  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_PACK_BUFFER, 4 * kCanvasSize * kCanvasSize, 0,
               GL_STATIC_READ);

  glReadPixels(0, 0, kCanvasSize, kCanvasSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glReadPixels(0, 0, kCanvasSize, kCanvasSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glReadPixels(0, 0, kCanvasSize, kCanvasSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ES3MapBufferRangeTest, TexImageAndSubImage2D) {
  if (ShouldSkipTest())
    return;

  const GLsizei kTextureSize = 4;
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, 4 * kTextureSize * kTextureSize, 0,
               GL_STATIC_READ);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  EXPECT_LT(0u, tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureSize, kTextureSize, GL_RGBA,
                  GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureSize, kTextureSize, GL_RGBA,
                  GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kTextureSize, kTextureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureSize, kTextureSize, GL_RGBA,
                  GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ES3MapBufferRangeTest, TexImageAndSubImage3D) {
  if (ShouldSkipTest())
    return;

  const GLsizei kTextureSize = 4;
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
  glBufferData(GL_PIXEL_UNPACK_BUFFER,
               4 * kTextureSize * kTextureSize * kTextureSize, 0,
               GL_STATIC_READ);

  GLuint tex = 0;
  glGenTextures(1, &tex);
  EXPECT_LT(0u, tex);
  glBindTexture(GL_TEXTURE_3D, tex);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTextureSize, kTextureSize,
               kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kTextureSize, kTextureSize,
                  kTextureSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 6, GL_MAP_READ_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTextureSize, kTextureSize,
               kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kTextureSize, kTextureSize,
                  kTextureSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, kTextureSize, kTextureSize,
               kTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, kTextureSize, kTextureSize,
                  kTextureSize, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// TODO(zmo): Add tests for CompressedTex* functions.

TEST_F(ES3MapBufferRangeTest, TransformFeedback) {
  if (ShouldSkipTest())
    return;

  GLuint transform_feedback = 0;
  glGenTransformFeedbacks(1, &transform_feedback);
  glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transform_feedback);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint transform_feedback_buffer[2];
  glGenBuffers(2, transform_feedback_buffer);
  EXPECT_LT(0u, transform_feedback_buffer[0]);
  EXPECT_LT(0u, transform_feedback_buffer[1]);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                   transform_feedback_buffer[0]);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 3 * 6 * sizeof(float), 0,
               GL_STATIC_READ);
  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1,
                   transform_feedback_buffer[1]);
  glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 2 * 6 * sizeof(float), 0,
               GL_STATIC_READ);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLint position_loc = 0;
  SetupTransformFeedbackProgram(&position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  GLuint buffer = GLTestHelper::SetupUnitQuad(position_loc);
  GLTestHelper::CheckGLError("no errors", __LINE__);
  EXPECT_LT(0u, buffer);

  glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBeginTransformFeedback(GL_TRIANGLES);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glBeginTransformFeedback(GL_TRIANGLES);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  // Bind the used buffer to another binding target and try to map it using
  // that target - should still fail.
  glBindBuffer(GL_PIXEL_PACK_BUFFER, transform_feedback_buffer[0]);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 6, GL_MAP_READ_BIT);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), glGetError());

  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEndTransformFeedback();
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 6, GL_MAP_WRITE_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);

  glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(ES3MapBufferRangeTest, GetBufferParameteriv) {
  if (ShouldSkipTest())
    return;

  GLuint buffer;
  glGenBuffers(1, &buffer);
  EXPECT_LT(0u, buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, 64, 0, GL_STATIC_READ);

  GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
  glMapBufferRange(GL_ARRAY_BUFFER, 16, 32, access);
  GLint param = 0;
  glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &param);
  EXPECT_EQ(access, static_cast<GLbitfield>(param));
  glUnmapBuffer(GL_ARRAY_BUFFER);
  param = 0;
  glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_FLAGS, &param);
  EXPECT_EQ(0u, static_cast<GLbitfield>(param));

  GLTestHelper::CheckGLError("no errors", __LINE__);
}

// TODO(zmo): add tests for uniform buffer mapping.

// TODO(zmo): add tests for CopyBufferSubData case.

}  // namespace gpu
