// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_SCREEN_SCOPED_PIXEL_BUFFER_OBJECT_H_
#define MEDIA_VIDEO_CAPTURE_SCREEN_SCOPED_PIXEL_BUFFER_OBJECT_H_

#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>
#include "base/basictypes.h"

namespace media {

class ScopedPixelBufferObject {
 public:
  ScopedPixelBufferObject();
  ~ScopedPixelBufferObject();

  bool Init(CGLContextObj cgl_context, int size_in_bytes);
  void Release();

  GLuint get() const { return pixel_buffer_object_; }

 private:
  CGLContextObj cgl_context_;
  GLuint pixel_buffer_object_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPixelBufferObject);
};

} // namespace media

#endif // MEDIA_VIDEO_CAPTURE_SCREEN_SCOPED_PIXEL_BUFFER_OBJECT_H_
