// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

size_t Resource::bytes() const {
  if (size_.IsEmpty())
    return 0;

  return MemorySizeBytes(size_, format_);
}

size_t Resource::BytesPerPixel(GLenum format) {
  size_t components_per_pixel = 0;
  size_t bytes_per_component = 1;
  switch (format) {
    case GL_RGBA:
    case GL_BGRA_EXT:
      components_per_pixel = 4;
      break;
    case GL_LUMINANCE:
      components_per_pixel = 1;
      break;
    default:
      NOTREACHED();
  }
  return components_per_pixel * bytes_per_component;
}

size_t Resource::MemorySizeBytes(gfx::Size size, GLenum format) {
  return BytesPerPixel(format) * size.width() * size.height();
}


}  // namespace cc
