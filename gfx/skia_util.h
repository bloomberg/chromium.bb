// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_SKIA_UTIL_H_
#define APP_GFX_SKIA_UTIL_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

class SkShader;

namespace gfx {

class Rect;

// Convert between Skia and gfx rect types.
SkRect RectToSkRect(const gfx::Rect& rect);
gfx::Rect SkRectToRect(const SkRect& rect);

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
//   paint.setShader(gfx::CreateGradientShader(0, 10, red, blue))->safeUnref();
//
// (The old shader in the paint, if any, needs to be freed, and safeUnref will
// handle the NULL case.)
SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color);

}  // namespace gfx;

#endif  // APP_GFX_SKIA_UTIL_H_
