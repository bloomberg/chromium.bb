// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/accelerated_surface_container_touch.h"

#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/transform.h"

AcceleratedSurfaceContainerTouch::AcceleratedSurfaceContainerTouch(
    ui::CompositorGL* compositor,
    const gfx::Size& size,
    uint64 surface_handle)
    : TextureGL(compositor, size),
      image_(NULL) {
  compositor_->MakeCurrent();

  image_ = eglCreateImageKHR(
      gfx::GLSurfaceEGL::GetDisplay(), EGL_NO_CONTEXT,
      EGL_NATIVE_PIXMAP_KHR, reinterpret_cast<void*>(surface_handle), NULL);

  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image_);
  glFlush();
}

AcceleratedSurfaceContainerTouch::~AcceleratedSurfaceContainerTouch() {
  eglDestroyImageKHR(gfx::GLSurfaceEGL::GetDisplay(), image_);
  glFlush();
}

void AcceleratedSurfaceContainerTouch::SetBitmap(
    const SkBitmap& bitmap,
    const gfx::Point& origin,
    const gfx::Size& overall_size) {
  NOTIMPLEMENTED();
}

void AcceleratedSurfaceContainerTouch::Draw(const ui::Transform& transform) {
  DCHECK(compositor_->program_no_swizzle());

  // Texture from GPU is flipped horizontally.
  ui::Transform flipped;
  flipped.SetScaleY(-1.0);
  flipped.SetTranslateY(size_.height());
  flipped.ConcatTransform(transform);

  DrawInternal(*compositor_->program_no_swizzle(), flipped);
}
