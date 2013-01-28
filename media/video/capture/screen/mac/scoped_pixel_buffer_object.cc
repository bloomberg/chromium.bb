// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/mac/scoped_pixel_buffer_object.h"

namespace media {

ScopedPixelBufferObject::ScopedPixelBufferObject()
    : cgl_context_(NULL),
      pixel_buffer_object_(0) {
}

ScopedPixelBufferObject::~ScopedPixelBufferObject() {
  Release();
}

bool ScopedPixelBufferObject::Init(CGLContextObj cgl_context,
                                      int size_in_bytes) {
  cgl_context_ = cgl_context;
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glGenBuffersARB(1, &pixel_buffer_object_);
  if (glGetError() == GL_NO_ERROR) {
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_);
    glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, size_in_bytes, NULL,
                    GL_STREAM_READ_ARB);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
    if (glGetError() != GL_NO_ERROR) {
      Release();
    }
  } else {
    cgl_context_ = NULL;
    pixel_buffer_object_ = 0;
  }
  return pixel_buffer_object_ != 0;
}

void ScopedPixelBufferObject::Release() {
  if (pixel_buffer_object_) {
    CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
    glDeleteBuffersARB(1, &pixel_buffer_object_);
    cgl_context_ = NULL;
    pixel_buffer_object_ = 0;
  }
}

} // namespace media
