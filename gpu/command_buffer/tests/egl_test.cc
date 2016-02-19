// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

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

class TestContext {
 public:
  TestContext() {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    eglInitialize(display_, nullptr, nullptr);

    static const EGLint configAttribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE
    };
    EGLint numConfigs;
    EGLConfig config;
    eglChooseConfig(display_, configAttribs, &config, 1, &numConfigs);

    static const EGLint surfaceAttribs[] = {
      EGL_WIDTH, 1,
      EGL_HEIGHT, 1,
      EGL_NONE
    };
    surface_ = eglCreatePbufferSurface(display_, config, surfaceAttribs);

    static const EGLint contextAttribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };
    context_ = eglCreateContext(display_, config, nullptr, contextAttribs);
  }

  ~TestContext() {
    eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display_, context_);
    eglDestroySurface(display_, surface_);
    eglTerminate(display_);
  }

  bool IsValid() const {
    return display_ != EGL_NO_DISPLAY &&
      context_ != EGL_NO_CONTEXT &&
      surface_ != EGL_NO_SURFACE;
  }

  void TestCallGL() {
    eglMakeCurrent(display_, surface_, surface_, context_);

    typedef GL_APICALL void GL_APIENTRY (*glEnableProc)(GLenum);

    auto address = eglGetProcAddress("glEnable");
    auto* glEnable = reinterpret_cast<glEnableProc>(address);
    glEnable(GL_BLEND);
  }

 private:
  TestContext(const TestContext&) = delete;
  TestContext& operator=(const TestContext&) = delete;

 private:
  EGLDisplay display_;
  EGLContext context_;
  EGLSurface surface_;
};

// Test case for a workaround for an issue in gles2_conform_support.
//
// The problem is that every call to eglGetDisplay(EGL_DEFAULT_DISPLAY)
// returns a new display object and the construction of each display
// calls to establish a thread local store variable in where to store the
// current command buffer context.
// Likewise the destructor of display calls to free the same tls variable.
//
// When skia (nanobench) uses multiple instances of command buffer
// based contexts and one display is destroyed the TLS variable
// is also destroyed and subsequent command buffer GL calls using another
// context instance end up accessing free'ed TLS variable.
//
// Note that technically there's also a problem in SkCommandBufferContext
// since it calls eglTerminate for each display, but this is because
// the current command buffer egl implementation requires this.
// Overall this functionality is not aligned with the EGL spefication.
//
// This testcase tests that we can create multiple command buffer contexts
// and dispose them without affecting the other.
TEST(Test, MultipleDisplays) {
  TestContext first;
  ASSERT_TRUE(first.IsValid());

  first.TestCallGL();

  {
    TestContext second;
    ASSERT_TRUE(second.IsValid());

    second.TestCallGL();
  }

  first.TestCallGL();
}

} // namespace gpu

