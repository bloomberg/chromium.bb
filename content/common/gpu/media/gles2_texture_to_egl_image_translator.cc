// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gles2_texture_to_egl_image_translator.h"

#include "base/logging.h"
#if defined(TOOLKIT_USES_GTK)
#include "base/message_pump_gtk.h"
#elif defined(USE_AURA)
#include "base/message_pump_aurax11.h"
#endif

namespace content {

Gles2TextureToEglImageTranslator::Gles2TextureToEglImageTranslator(
    bool use_backing_pixmaps)
    : use_backing_pixmaps_(use_backing_pixmaps) {
}


Gles2TextureToEglImageTranslator::~Gles2TextureToEglImageTranslator() {
}

EGLImageKHR Gles2TextureToEglImageTranslator::TranslateToEglImage(
    EGLDisplay egl_display, EGLContext egl_context,
    uint32 texture,
    const gfx::Size& dimensions) {
  EGLImageKHR hEglImage;
  if (use_backing_pixmaps_) {
    EGLint image_attrs[] = { EGL_IMAGE_PRESERVED_KHR, 1 , EGL_NONE };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    Display* x_display = base::MessagePumpForUI::GetDefaultXDisplay();
    Pixmap pixmap = XCreatePixmap(x_display, RootWindow(x_display, 0),
                                  dimensions.width(),
                                  dimensions.height(), 32);

    hEglImage = eglCreateImageKHR(egl_display,
                                  EGL_NO_CONTEXT,
                                  EGL_NATIVE_PIXMAP_KHR,
                                  (EGLClientBuffer)pixmap,
                                  image_attrs);

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, hEglImage);
    bool inserted = eglimage_pixmap_.insert(std::make_pair(
                                            hEglImage, pixmap)).second;
    DCHECK(inserted);
  } else {
    EGLint attrib = EGL_NONE;
    // Create an EGLImage
    hEglImage = eglCreateImageKHR(egl_display,
                                  egl_context,
                                  EGL_GL_TEXTURE_2D_KHR,
                                  reinterpret_cast<EGLClientBuffer>(texture),
                                  &attrib);
  }
  CHECK(hEglImage) << "Failed to eglCreateImageKHR for " << texture
                   << ", error: 0x" << std::hex << eglGetError();
  return hEglImage;
}

uint32 Gles2TextureToEglImageTranslator::TranslateToTexture(
    EGLImageKHR egl_image) {
  // TODO(vhiremath@nvidia.com)
  // Fill in the appropriate implementation.
  GLuint texture = 0;
  NOTIMPLEMENTED();
  return texture;
}

void Gles2TextureToEglImageTranslator::DestroyEglImage(
    EGLDisplay egl_display, EGLImageKHR egl_image) {
  // Clients of this class will call this method for each EGLImage handle.
  // Actual destroying of the handles is done here.
  eglDestroyImageKHR(egl_display, egl_image);

  if (use_backing_pixmaps_) {
    ImagePixmap::iterator it = eglimage_pixmap_.find(egl_image);
    CHECK(it != eglimage_pixmap_.end());
    Pixmap pixmap = it->second;
    eglimage_pixmap_.erase(it);
    Display* x_display = base::MessagePumpForUI::GetDefaultXDisplay();
    XFreePixmap(x_display, pixmap);
  }
}

}  // namespace content
