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

class GLChromiumFramebufferMultisampleTest : public testing::Test {
 protected:
  virtual void SetUp() {
    gl_.Initialize(GLManager::Options());
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  GLManager gl_;
};

// Test that GL is at least minimally working.
TEST_F(GLChromiumFramebufferMultisampleTest, CachedBindingsTest) {
  if (!GLTestHelper::HasExtension("GL_CHROMIUM_framebuffer_multisample")) {
    return;
  }

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // If the caching is bad the second call to glBindFramebuffer will do nothing.
  // which means the draw buffer is bad and will not return
  // GL_FRAMEBUFFER_COMPLETE and rendering will generate an error.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  glClear(GL_COLOR_BUFFER_BIT);
  GLTestHelper::CheckGLError("no errors", __LINE__);
}

}  // namespace gpu

