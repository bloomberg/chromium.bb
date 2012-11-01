// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_GEOMETRY_H_
#define CC_GEOMETRY_H_

#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"
#include <cmath>

namespace cc {

inline gfx::Size ClampSizeFromAbove(gfx::Size s, gfx::Size other) {
  return gfx::Size(s.width() < other.width() ? s.width() : other.width(),
                   s.height() < other.height() ? s.height() : other.height());
}

}  // namespace cc

#endif // CC_GEOMETRY_H_
