// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PLATFORM_COLOR_H_
#define CC_RESOURCES_PLATFORM_COLOR_H_

#include "base/basictypes.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace cc {

class PlatformColor {
 public:
  enum SourceDataFormat {
    SOURCE_FORMAT_RGBA8,
    SOURCE_FORMAT_BGRA8
  };

  static SourceDataFormat Format() {
    return SK_B32_SHIFT ? SOURCE_FORMAT_RGBA8 : SOURCE_FORMAT_BGRA8;
  }

  // Returns the most efficient texture format for this platform.
  static GLenum BestTextureFormat(WebKit::WebGraphicsContext3D* context,
                                  bool supports_bgra8888) {
    GLenum texture_format = GL_RGBA;
    switch (Format()) {
      case SOURCE_FORMAT_RGBA8:
        break;
      case SOURCE_FORMAT_BGRA8:
        if (supports_bgra8888)
          texture_format = GL_BGRA_EXT;
        break;
      default:
        NOTREACHED();
        break;
    }
    return texture_format;
  }

  // Return true if the given texture format has the same component order
  // as the color on this platform.
  static bool SameComponentOrder(GLenum texture_format) {
    switch (Format()) {
      case SOURCE_FORMAT_RGBA8:
        return texture_format == GL_RGBA;
      case SOURCE_FORMAT_BGRA8:
        return texture_format == GL_BGRA_EXT;
      default:
        NOTREACHED();
        return false;
    }
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformColor);
};

}  // namespace cc

#endif  // CC_RESOURCES_PLATFORM_COLOR_H_
