// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GLES2_TEXTURE_TO_EGL_IMAGE_TRANSLATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GLES2_TEXTURE_TO_EGL_IMAGE_TRANSLATOR_H_

#include <map>

#include "base/basictypes.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"
#include "third_party/angle/include/GLES2/gl2ext.h"

// Class to wrap egl-opengles related operations.
// PPAPI will give the textures to OmxVideoDecodeAccelerator.
// OmxVideoDecodeAccelerator will use this class to convert
// these texture into EGLImage and back.
class Gles2TextureToEglImageTranslator {
 public:
  Gles2TextureToEglImageTranslator(bool use_backing_pixmaps);
  ~Gles2TextureToEglImageTranslator();

  // Translates texture into EGLImage and back.
  EGLImageKHR TranslateToEglImage(EGLDisplay egl_display,
                                  EGLContext egl_context,
                                  uint32 texture,
                                  const gfx::Size& dimensions);
  uint32 TranslateToTexture(EGLImageKHR egl_image);
  void DestroyEglImage(EGLDisplay egl_display, EGLImageKHR egl_image);

 private:
  bool use_backing_pixmaps_;
  typedef std::map<EGLImageKHR, Pixmap> ImagePixmap;
  ImagePixmap eglimage_pixmap_;
  DISALLOW_COPY_AND_ASSIGN(Gles2TextureToEglImageTranslator);
};

#endif  // CONTENT_COMMON_GPU_MEDIA_GLES2_TEXTURE_TO_EGL_IMAGE_TRANSLATOR_H_
