// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BUBBLE_BORDER_H_
#define CHROME_BROWSER_VIEWS_BUBBLE_BORDER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "views/background.h"
#include "views/border.h"

class SkBitmap;

// Renders a round-rect border, with optional arrow (off by default), and a
// custom dropshadow.  This can be used to produce floating "bubble" objects.
class BubbleBorder : public views::Border {
 public:
  // Possible locations for the (optional) arrow.
  enum ArrowLocation {
    NONE,
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT
  };

  BubbleBorder() : arrow_location_(NONE), background_color_(SK_ColorWHITE) {
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

  // Sets the location for the arrow.
  void set_arrow_location(ArrowLocation arrow_location) {
    arrow_location_ = arrow_location;
  }

  // Sets the background color for the arrow body.  This is irrelevant if you do
  // not also set the arrow location to something other than NONE.
  void set_background_color(SkColor background_color) {
    background_color_ = background_color;
  }
  SkColor background_color() const { return background_color_; }

  // For borders with an arrow, gives the desired bounds (in screen coordinates)
  // given the rect to point to and the size of the contained contents.  This
  // depends on the arrow location, so if you change that, you should call this
  // again to find out the new coordinates.
  //
  // For borders without an arrow, gives the bounds with the content centered
  // underneath the supplied rect.
  gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                      const gfx::Size& contents_size) const;

  // Overridden from views::Border:
  virtual void GetInsets(gfx::Insets* insets) const;

 private:
  // Loads images if necessary.
  static void InitClass();

  virtual ~BubbleBorder() { }

  // Returns true if there is an arrow and it is positioned on the top edge.
  bool arrow_is_top() const {
    return (arrow_location_ == TOP_LEFT) || (arrow_location_ == TOP_RIGHT);
  }

  // Returns true if there is an arrow and it is positioned on the left side.
  bool arrow_is_left() const {
    return (arrow_location_ == TOP_LEFT) || (arrow_location_ == BOTTOM_LEFT);
  }

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
  static SkBitmap* top_arrow_;
  static SkBitmap* bottom_arrow_;

  static int arrow_x_offset_;

  ArrowLocation arrow_location_;
  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorder);
};

// A Background that clips itself to the specified BubbleBorder and uses
// the background color of the BubbleBorder.
class BubbleBackground : public views::Background {
 public:
  explicit BubbleBackground(BubbleBorder* border) : border_(border) {}

  // Background overrides.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const;

 private:
  BubbleBorder* border_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBackground);
};

#endif  // CHROME_BROWSER_VIEWS_BUBBLE_BORDER_H_
