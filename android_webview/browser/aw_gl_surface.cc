// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_gl_surface.h"

#include "android_webview/browser/scoped_app_gl_state_restore.h"

namespace android_webview {

AwGLSurface::AwGLSurface() : size_(1, 1) {}

AwGLSurface::~AwGLSurface() {}

void AwGLSurface::Destroy() {
}

bool AwGLSurface::IsOffscreen() {
  return false;
}

unsigned int AwGLSurface::GetBackingFramebufferObject() {
  return ScopedAppGLStateRestore::Current()->framebuffer_binding_ext();
}

gfx::SwapResult AwGLSurface::SwapBuffers(PresentationCallback callback) {
  // TODO(penghuang): Provide presentation feedback. https://crbug.com/776877
  return gfx::SwapResult::SWAP_ACK;
}

gfx::Size AwGLSurface::GetSize() {
  return size_;
}

void* AwGLSurface::GetHandle() {
  return NULL;
}

void* AwGLSurface::GetDisplay() {
  return NULL;
}

gl::GLSurfaceFormat AwGLSurface::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool AwGLSurface::Resize(const gfx::Size& size,
                         float scale_factor,
                         ColorSpace color_space,
                         bool has_alpha) {
  size_ = size;
  return true;
}

}  // namespace android_webview
