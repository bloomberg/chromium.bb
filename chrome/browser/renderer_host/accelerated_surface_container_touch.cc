// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "accelerated_surface_container_touch.h"

#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface_egl.h"

AcceleratedSurfaceContainerTouch::AcceleratedSurfaceContainerTouch(
    uint64 surface_handle)
    : image_(NULL),
      texture_(0) {
  image_ = eglCreateImageKHR(
      gfx::GLSurfaceEGL::GetDisplay(), EGL_NO_CONTEXT,
      EGL_NATIVE_PIXMAP_KHR, (void*) surface_handle, NULL);

  glGenTextures(1, &texture_);

  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
}

AcceleratedSurfaceContainerTouch::~AcceleratedSurfaceContainerTouch() {
  glDeleteTextures(1, &texture_);
  eglDestroyImageKHR(gfx::GLSurfaceEGL::GetDisplay(), image_);
}
