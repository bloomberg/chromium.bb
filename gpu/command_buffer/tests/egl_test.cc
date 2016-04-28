// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <EGL/egl.h>

// This file tests EGL basic interface for command_buffer_gles2, the mode of
// command buffer where the code is compiled as a standalone dynamic library and
// exposed through EGL API.
namespace gpu {

using testing::Test;

TEST_F(Test, BasicEGLInitialization) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  ASSERT_NE(display, EGL_NO_DISPLAY);

  // Test for no crash even though passing nullptrs for major, minor.
  EGLBoolean success = eglInitialize(display, nullptr, nullptr);
  ASSERT_TRUE(success);

  EGLint major = 0;
  EGLint minor = 0;
  success = eglInitialize(display, &major, &minor);
  ASSERT_TRUE(success);
  ASSERT_EQ(major, 1);
  ASSERT_EQ(minor, 4);

  success = eglTerminate(display);
  ASSERT_TRUE(success);
}

}  // namespace gpu
