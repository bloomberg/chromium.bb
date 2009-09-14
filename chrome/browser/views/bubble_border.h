// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BUBBLE_BORDER_H_
#define CHROME_BROWSER_VIEWS_BUBBLE_BORDER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "views/border.h"

class SkBitmap;

// Renders a round-rect border and a custom dropshadow.  This can be used to
// produce floating "bubble" objects.
class BubbleBorder : public views::Border {
 public:
  BubbleBorder() {
    InitClass();
  }

  // Returns the radius of the corner of the border.
  static int GetCornerRadius() {
    // We can't safely calculate a border radius by comparing the sizes of the
    // side and corner images, because either may have been extended in various
    // directions in order to do more subtle dropshadow fading or other effects.
    // So we hardcode the most accurate value.
    return 4;
  }

  // Overridden from views::Border:
  virtual void GetInsets(gfx::Insets* insets) const;

 private:
  // Loads images if necessary.
  static void InitClass();

  virtual ~BubbleBorder() { }

  // Overridden from views::Border:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const;

  // Border graphics.
  static SkBitmap* left_;
  static SkBitmap* top_left_;
  static SkBitmap* top_;
  static SkBitmap* top_right_;
  static SkBitmap* right_;
  static SkBitmap* bottom_right_;
  static SkBitmap* bottom_;
  static SkBitmap* bottom_left_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorder);
};

#endif  // #ifndef CHROME_BROWSER_VIEWS_BUBBLE_BORDER_H_