// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gles2_texture_to_egl_image_translator.h"

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

Gles2TextureToEglImageTranslator::Gles2TextureToEglImageTranslator(
    Display* display,
    int32 gles2_context_id)
    : size_(320, 240),
      egl_display_((EGLDisplay)0x1/*display*/),
      egl_context_((EGLContext)0x368b8001/*gles2_context_id*/) {
  // TODO(vhiremath@nvidia.com)
  // Replace the above hard coded values with appropriate variables.
  // size_/egl_display_/egl_context_.
  // These should be initiated from the App.
  if (!AreEGLExtensionsInitialized()) {
    LOG(DFATAL) << "Failed to get EGL extensions";
    return;
  }
}


Gles2TextureToEglImageTranslator::~Gles2TextureToEglImageTranslator() {
}

EGLImageKHR Gles2TextureToEglImageTranslator::TranslateToEglImage(
    uint32 texture) {
  EGLint attrib = EGL_NONE;
  if (!egl_create_image_khr)
    return EGL_NO_IMAGE_KHR;
  // Create an EGLImage
  EGLImageKHR hEglImage = egl_create_image_khr(
                              egl_display_,
                              egl_context_,
                              EGL_GL_TEXTURE_2D_KHR,
                              reinterpret_cast<EGLClientBuffer>(texture),
                              &attrib);
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

void Gles2TextureToEglImageTranslator::DestroyEglImage(EGLImageKHR egl_image) {
  // Clients of this class will call this method for each EGLImage handle.
  // Actual destroying of the handles is done here.
  if (!egl_destroy_image_khr) {
    LOG(ERROR) << "egl_destroy_image_khr failed";
    return;
  }
  egl_destroy_image_khr(egl_display_, egl_image);
}


