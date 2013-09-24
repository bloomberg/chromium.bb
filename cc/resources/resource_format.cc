// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_format.h"

namespace cc {

SkBitmap::Config SkBitmapConfigFromFormat(ResourceFormat format) {
  switch (format) {
    case RGBA_4444:
      return SkBitmap::kARGB_4444_Config;
    case RGBA_8888:
    case BGRA_8888:
      return SkBitmap::kARGB_8888_Config;
    case LUMINANCE_8:
    case RGB_565:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return SkBitmap::kARGB_8888_Config;
}

}  // namespace cc
