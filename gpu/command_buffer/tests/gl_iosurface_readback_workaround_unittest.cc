// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

#if defined(OS_MACOSX)
// A test that exercises the glReadPixels workaround for IOSurface backed
// textures.
class GLIOSurfaceReadbackWorkaroundTest : public testing::Test {
 public:
  GLIOSurfaceReadbackWorkaroundTest() {}

 protected:
  void SetUp() override {
      base::CommandLine command_line(0, NULL);
      command_line.AppendSwitchASCII(
          switches::kGpuDriverBugWorkarounds,
          base::IntToString(gpu::IOSURFACE_READBACK_WORKAROUND));
      gl_.InitializeWithCommandLine(GLManager::Options(), &command_line);
      gl_.set_use_iosurface_memory_buffers(true);
      DCHECK(gl_.workarounds().iosurface_readback_workaround);
  }

  void TearDown() override {
    GLTestHelper::CheckGLError("no errors", __LINE__);
    gl_.Destroy();
  }

  GLManager gl_;
};

TEST_F(GLIOSurfaceReadbackWorkaroundTest, ReadPixels) {
  int width = 1;
  int height = 1;
  GLuint source_texture = 0;
  GLenum source_target = GL_TEXTURE_RECTANGLE_ARB;
  glGenTextures(1, &source_texture);
  glBindTexture(source_target, source_texture);
  glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  GLuint image_id = glCreateGpuMemoryBufferImageCHROMIUM(
      width, height, GL_RGBA, GL_READ_WRITE_CHROMIUM);
  ASSERT_NE(0u, image_id);
  glBindTexImage2DCHROMIUM(source_target, image_id);

  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, source_target, source_texture, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));

  glClearColor(33.0 / 255.0, 44.0 / 255.0, 55.0 / 255.0, 66.0 / 255.0);
  glClear(GL_COLOR_BUFFER_BIT);
  const uint8_t expected[4] = {33, 44, 55, 66};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */, expected));

  glClearColor(14.0 / 255.0, 15.0 / 255.0, 16.0 / 255.0, 17.0 / 255.0);
  glClear(GL_COLOR_BUFFER_BIT);
  const uint8_t expected2[4] = {14, 15, 16, 17};
  EXPECT_TRUE(
      GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */, expected2));

  glReleaseTexImage2DCHROMIUM(source_target, image_id);
  glDestroyImageCHROMIUM(image_id);
  glDeleteTextures(1, &source_texture);
  glDeleteFramebuffers(1, &framebuffer);
}

#endif  // defined(OS_MACOSX)

}  // namespace gpu
