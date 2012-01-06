// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/gles2_texture_to_egl_image_translator.h"

#include "base/logging.h"

// Get EGL extension functions.
static PFNEGLCREATEIMAGEKHRPROC egl_create_image_khr =
    reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
        eglGetProcAddress("eglCreateImageKHR"));
static PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image_khr =
    reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
        eglGetProcAddress("eglDestroyImageKHR"));

static bool AreEGLExtensionsInitialized() {
  return (egl_create_image_khr && egl_destroy_image_khr);
}

Gles2TextureToEglImageTranslator::Gles2TextureToEglImageTranslator() {
  if (!AreEGLExtensionsInitialized()) {
    LOG(DFATAL) << "Failed to get EGL extensions";
    return;
  }
  CHECK_EQ(eglGetError(), EGL_SUCCESS);
}


Gles2TextureToEglImageTranslator::~Gles2TextureToEglImageTranslator() {
}

EGLImageKHR Gles2TextureToEglImageTranslator::TranslateToEglImage(
    EGLDisplay egl_display, EGLContext egl_context, uint32 texture) {
  EGLint attrib = EGL_NONE;
  if (!egl_create_image_khr)
    return EGL_NO_IMAGE_KHR;
  // Create an EGLImage
  EGLImageKHR hEglImage = egl_create_image_khr(
      egl_display,
      egl_context,
      EGL_GL_TEXTURE_2D_KHR,
      reinterpret_cast<EGLClientBuffer>(texture),
      &attrib);
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
  if (!egl_destroy_image_khr) {
    DLOG(ERROR) << "egl_destroy_image_khr failed";
    return;
  }
  egl_destroy_image_khr(egl_display, egl_image);
}
