// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLReadbackTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl_.Initialize(GLManager::Options());
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  GLManager gl_;
};


TEST_F(GLReadbackTest, ReadPixelsWithPBO) {
  const GLint kBytesPerPixel = 4;
  const GLint kWidth = 2;
  const GLint kHeight = 2;

  GLuint b;
  glClearColor(0.0, 0.0, 1.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glGenBuffers(1, &b);
  glBindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, b);
  glBufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
               kWidth * kHeight * kBytesPerPixel,
               NULL,
               GL_STREAM_READ);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  unsigned char *data = static_cast<unsigned char *>(
      glMapBufferCHROMIUM(
          GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
          GL_READ_ONLY));
  EXPECT_TRUE(data);
  EXPECT_EQ(data[0], 0);   // red
  EXPECT_EQ(data[1], 0);   // green
  EXPECT_EQ(data[2], 255); // blue
  glUnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
  glBindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  glDeleteBuffers(1, &b);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

TEST_F(GLReadbackTest, ReadPixelsWithPBOAndQuery) {
  const GLint kBytesPerPixel = 4;
  const GLint kWidth = 2;
  const GLint kHeight = 2;

  GLuint b, q;
  glClearColor(0.0, 0.0, 1.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  glGenBuffers(1, &b);
  glGenQueriesEXT(1, &q);
  glBindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, b);
  glBufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
               kWidth * kHeight * kBytesPerPixel,
               NULL,
               GL_STREAM_READ);
  glBeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, q);
  glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glEndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  glFlush();
  unsigned int done = 0;
  while (!done) {
    glGetQueryObjectuivEXT(q, GL_QUERY_RESULT_AVAILABLE_EXT, &done);
  }

  // TODO(hubbe): Check that glMapBufferCHROMIUM does not block here.
  unsigned char *data = static_cast<unsigned char *>(
      glMapBufferCHROMIUM(
          GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
          GL_READ_ONLY));
  EXPECT_TRUE(data);
  EXPECT_EQ(data[0], 0);   // red
  EXPECT_EQ(data[1], 0);   // green
  EXPECT_EQ(data[2], 255); // blue
  glUnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);
  glBindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  glDeleteBuffers(1, &b);
  glDeleteQueriesEXT(1, &q);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu
