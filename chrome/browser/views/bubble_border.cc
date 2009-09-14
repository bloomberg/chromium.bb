// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/views/bubble_border.h"

#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

// static
SkBitmap* BubbleBorder::left_ = NULL;
SkBitmap* BubbleBorder::top_left_ = NULL;
SkBitmap* BubbleBorder::top_ = NULL;
SkBitmap* BubbleBorder::top_right_ = NULL;
SkBitmap* BubbleBorder::right_ = NULL;
SkBitmap* BubbleBorder::bottom_right_ = NULL;
SkBitmap* BubbleBorder::bottom_ = NULL;
SkBitmap* BubbleBorder::bottom_left_ = NULL;

void BubbleBorder::GetInsets(gfx::Insets* insets) const {
  // The left, right and bottom edge image sizes define our insets. The corner
  // images don't determine this because they can extend inside the border (onto
  // the contained contents).
  insets->Set(top_->height(), left_->width(), bottom_->height(),
              right_->width());
}

// static
void BubbleBorder::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    // Load images.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    left_ = rb.GetBitmapNamed(IDR_BUBBLE_L);
    top_left_ = rb.GetBitmapNamed(IDR_BUBBLE_TL);
    top_ = rb.GetBitmapNamed(IDR_BUBBLE_T);
    top_right_ = rb.GetBitmapNamed(IDR_BUBBLE_TR);
    right_ = rb.GetBitmapNamed(IDR_BUBBLE_R);
    bottom_right_ = rb.GetBitmapNamed(IDR_BUBBLE_BR);
    bottom_ = rb.GetBitmapNamed(IDR_BUBBLE_B);
    bottom_left_ = rb.GetBitmapNamed(IDR_BUBBLE_BL);

    initialized = true;
  }
}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) const {
  // Convenience shorthand variables
  int width = view.width();
  int tl_width = top_left_->width();
  int tl_height = top_left_->height();
  int t_height = top_->height();
  int tr_width = top_right_->width();
  int tr_height = top_right_->height();
  int r_width = right_->width();
  int br_width = bottom_right_->width();
  int br_height = bottom_right_->height();
  int b_height = bottom_->height();
  int bl_width = bottom_left_->width();
  int bl_height = bottom_left_->height();

  /* The variables below can be confusing; here's what they mean:
   *
   * border_top∙∙∙∙∙∙∙┌────┬────────┬────┐
   *                  │  / │--------│ \  │
   * top∙∙∙∙∙∙∙∙∙∙∙∙∙∙│ /  ├────────┤  \ │
   *   tl_bottom∙∙∙∙∙∙├───┬┘        └┬───┤∙∙∙∙∙∙tr_bottom
   *                  │ | │          │ | │
   *                  │ | │          │ | │
   *                  │ | │          │ | │
   *   bl_y∙∙∙∙∙∙∙∙∙∙∙├───┴┐        ┌┴───┤∙∙∙∙∙∙br_y
   * bottom∙∙∙∙∙∙∙∙∙∙∙│ \  ├────────┤  / │
   *                  │  \ │--------│ /  │
   * border_bottom∙∙∙∙└────┴────────┴────┘
   */

  gfx::Insets insets;
  GetInsets(&insets);
  int top = insets.top();
  int border_top = top - t_height;
  int tl_bottom = border_top + tl_height;
  int tr_bottom = border_top + tr_height;
  int bottom = view.height() - insets.bottom();
  int border_bottom = bottom + b_height;
  int bl_y = border_bottom - bl_height;
  int br_y = border_bottom - br_height;

  // Top left corner
  canvas->DrawBitmapInt(*top_left_, 0, border_top);

  // Top edge
  canvas->TileImageInt(*top_, tl_width, border_top, width - tl_width - tr_width,
                       t_height);

  // Top right corner
  canvas->DrawBitmapInt(*top_right_, width - tr_width, border_top);

  // Right edge
  canvas->TileImageInt(*right_, width - r_width, tr_bottom, r_width,
                       br_y - tr_bottom);

  // Bottom right corner
  canvas->DrawBitmapInt(*bottom_right_, width - br_width, br_y);

  // Bottom edge
  canvas->TileImageInt(*bottom_, bl_width, bottom, width - bl_width - br_width,
                       b_height);
  // Bottom left corner
  canvas->DrawBitmapInt(*bottom_left_, 0, bl_y);

  // Left edge
  canvas->TileImageInt(*left_, 0, tl_bottom, left_->width(), bl_y - tl_bottom);
}
