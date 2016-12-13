// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BACKGROUND_WITH_1_PX_BORDER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BACKGROUND_WITH_1_PX_BORDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/background.h"

namespace gfx {
class Canvas;
}

namespace views {
class View;
}

// BackgroundWith1PxBorder renders a solid background color, with a one pixel
// border with rounded corners. This accounts for the scaling of the canvas, so
// that the border is one pixel regardless of display scaling.
class BackgroundWith1PxBorder : public views::Background {
 public:
  // Corner radius of the inside edge of the roundrect border stroke.
  static constexpr int kCornerRadius = 2;

  // The thickness of the location bar's border in DIP.
  static constexpr int kLocationBarBorderThicknessDip = 1;

  BackgroundWith1PxBorder(SkColor background, SkColor border);

  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  // Color for the one pixel border.
  SkColor border_color_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundWith1PxBorder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BACKGROUND_WITH_1_PX_BORDER_H_
