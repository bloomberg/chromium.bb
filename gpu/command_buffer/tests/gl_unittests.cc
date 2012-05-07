// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "gpu/command_buffer/tests/gl_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GLTest : public testing::Test {
 protected:
  GLTest() : gl_(NULL, NULL) {
  }

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
  uint8 pixels[4] = { 0, };
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  EXPECT_EQ(0u, pixels[0]);
  EXPECT_EQ(255u, pixels[1]);
  EXPECT_EQ(0u, pixels[2]);
  EXPECT_EQ(255u, pixels[3]);
}

}  // namespace gpu

