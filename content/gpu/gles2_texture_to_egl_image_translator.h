// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GLES2_TEXTURE_TO_EGL_IMAGE_TRANSLATOR_H_
#define CONTENT_GPU_GLES2_TEXTURE_TO_EGL_IMAGE_TRANSLATOR_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/basictypes.h"
#include "media/video/picture.h"

// Class to wrap egl-opengles related operations.
// PPAPI will give the textures to OmxVideoDecodeAccelerator.
// OmxVideoDecodeAccelerator will use this class to convert
// these texture into EGLImage and back.
class Gles2TextureToEglImageTranslator {
 public:
  Gles2TextureToEglImageTranslator(Display* display, int32 gles2_context_id);
  ~Gles2TextureToEglImageTranslator();

  // Translates texture into EGLImage and back.
  EGLImageKHR TranslateToEglImage(uint32 texture);
  uint32 TranslateToTexture(EGLImageKHR egl_image);
  void DestroyEglImage(EGLImageKHR egl_image);

 private:
  int32 gles2_context_id_;
  gfx::Size size_;
  EGLDisplay egl_display_;
  EGLContext egl_context_;
  EGLSurface egl_surface_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Gles2TextureToEglImageTranslator);
};

#endif  // CONTENT_GPU_GLES2_TEXTURE_TO_EGL_IMAGE_TRANSLATOR_H_


