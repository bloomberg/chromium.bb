// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/gl_test_environment.h"

#include "ui/gl/test/gl_image_test_template.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace vr {

GlTestEnvironment::GlTestEnvironment(const gfx::Size frame_buffer_size) {
  // Setup offscreen GL context.
  gl::GLImageTestSupport::InitializeGL();
  surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  context_->MakeCurrent(surface_.get());

  if (gl::GLContext::GetCurrent()->GetVersionInfo()->IsAtLeastGL(3, 3)) {
    // To avoid glGetVertexAttribiv(0, ...) failing.
    glGenVertexArraysOES(1, &vao_);
    glBindVertexArrayOES(vao_);
  }

  frame_buffer_ = gl::GLTestHelper::SetupFramebuffer(
      frame_buffer_size.width(), frame_buffer_size.height());
  glBindFramebufferEXT(GL_FRAMEBUFFER, frame_buffer_);

  // Create depth buffer.
  glGenRenderbuffersEXT(1, &depth_render_buffer_);
  glBindRenderbufferEXT(GL_RENDERBUFFER, depth_render_buffer_);
  glRenderbufferStorageEXT(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                           frame_buffer_size.width(),
                           frame_buffer_size.height());
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_RENDERBUFFER, depth_render_buffer_);
  glBindRenderbufferEXT(GL_RENDERBUFFER, 0);
}

GlTestEnvironment::~GlTestEnvironment() {
  glDeleteFramebuffersEXT(1, &frame_buffer_);
  if (vao_) {
    glDeleteVertexArraysOES(1, &vao_);
  }
  context_->ReleaseCurrent(surface_.get());
  context_ = nullptr;
  surface_ = nullptr;
  gl::GLImageTestSupport::CleanupGL();
}

GLuint GlTestEnvironment::GetFrameBufferForTesting() {
  return frame_buffer_;
}

}  // namespace vr
