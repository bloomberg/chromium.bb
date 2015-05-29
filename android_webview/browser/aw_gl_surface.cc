// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_gl_surface.h"

namespace android_webview {

AwGLSurface::AwGLSurface() : fbo_(0) {}

AwGLSurface::~AwGLSurface() {}

void AwGLSurface::Destroy() {
}

bool AwGLSurface::IsOffscreen() {
  return false;
}

unsigned int AwGLSurface::GetBackingFrameBufferObject() {
  return fbo_;
}

bool AwGLSurface::SwapBuffers() {
  return true;
}

gfx::Size AwGLSurface::GetSize() {
  return gfx::Size(1, 1);
}

void* AwGLSurface::GetHandle() {
  return NULL;
}

void* AwGLSurface::GetDisplay() {
  return NULL;
}

void AwGLSurface::SetBackingFrameBufferObject(unsigned int fbo) {
  fbo_ = fbo;
}

void AwGLSurface::ResetBackingFrameBufferObject() {
  fbo_ = 0;
}

}  // namespace android_webview
