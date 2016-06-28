// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"

namespace gpu {

class GLNativeGMBTest : public testing::Test {
 protected:
  void SetUp() override {
    gl_.Initialize(GLManager::Options());
    image_factory_.SetRequiredTextureType(GL_TEXTURE_RECTANGLE_ARB);
  }

  void TearDown() override {
    gl_.Destroy();
  }

  GLManager gl_;
  TextureImageFactory image_factory_;
};

TEST_F(GLNativeGMBTest, TestNativeGMBBackbufferWithDifferentConfigurations) {
  if (!GLTestHelper::HasExtension("GL_ARB_texture_rectangle")) {
    LOG(INFO) << "GL_ARB_texture_rectangle not supported. Skipping test...";
    return;
  }

  for (int has_alpha = 0; has_alpha <= 1; ++has_alpha) {
    for (int msaa = 0; msaa <= 1; ++msaa) {
      GLManager::Options options;
      options.image_factory = &image_factory_;
      options.multisampled = msaa == 1;
      options.backbuffer_alpha = has_alpha == 1;
      options.enable_arb_texture_rectangle = true;

      GLManager gl;
      gl.Initialize(options);
      gl.MakeCurrent();

      glResizeCHROMIUM(10, 10, 1, true);
      glClearColor(0.0f, 0.25f, 0.5f, 0.7f);
      glClear(GL_COLOR_BUFFER_BIT);

      uint8_t pixel[4];
      memset(pixel, 0, 4);
      glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
      EXPECT_NEAR(0u, pixel[0], 2);
      EXPECT_NEAR(64u, pixel[1], 2);
      EXPECT_NEAR(127u, pixel[2], 2);
      uint8_t alpha = has_alpha ? 178 : 255;
      EXPECT_NEAR(alpha, pixel[3], 2);

      ::gles2::GetGLContext()->SwapBuffers();
      glClearColor(0.1f, 0.2f, 0.3f, 0.4f);
      glClear(GL_COLOR_BUFFER_BIT);
      memset(pixel, 0, 4);
      glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
      EXPECT_NEAR(25u, pixel[0], 2);
      EXPECT_NEAR(51u, pixel[1], 2);
      EXPECT_NEAR(76u, pixel[2], 2);
      uint8_t alpha2 = has_alpha ? 102 : 255;
      EXPECT_NEAR(alpha2, pixel[3], 2);

      gl.Destroy();
    }
  }
}

}  // namespace gpu
