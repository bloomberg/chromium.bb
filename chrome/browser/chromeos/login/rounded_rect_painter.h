// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ROUNDED_RECT_PAINTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ROUNDED_RECT_PAINTER_H_

#include "base/basictypes.h"
#include "views/painter.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
}

namespace chromeos {

// This Painter can be used to draw a background consistent cross all login
// screens. It draws a rect with padding, shadow and rounded corners.
class RoundedRectPainter : public views::Painter {
 public:
  RoundedRectPainter(
      int padding, SkColor padding_color,
      int shadow, SkColor shadow_color,
      int corner_radius,
      SkColor top_color, SkColor bottom_color);

  virtual void Paint(int w, int h, gfx::Canvas* canvas);

 private:
  int padding_;
  SkColor padding_color_;
  int shadow_;
  SkColor shadow_color_;
  int corner_radius_;
  SkColor top_color_;
  SkColor bottom_color_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectPainter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ROUNDED_RECT_PAINTER_H_
