// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"

namespace cc {

bool PaintFlags::IsSimpleOpacity() const {
  uint32_t color = getColor();
  if (SK_ColorTRANSPARENT != SkColorSetA(color, SK_AlphaTRANSPARENT))
    return false;
  if (!isSrcOver())
    return false;
  if (getLooper())
    return false;
  if (getPathEffect())
    return false;
  if (getShader())
    return false;
  if (getMaskFilter())
    return false;
  if (getColorFilter())
    return false;
  if (getImageFilter())
    return false;
  return true;
}

bool PaintFlags::SupportsFoldingAlpha() const {
  if (!isSrcOver())
    return false;
  if (getColorFilter())
    return false;
  if (getImageFilter())
    return false;
  if (getLooper())
    return false;
  return true;
}

}  // namespace cc
