// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/tabs/side_tab.h"

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas.h"
#include "gfx/path.h"
#include "gfx/skia_util.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"

namespace {
const int kVerticalTabHeight = 27;
const int kIconSize = 16;
const int kIconTitleSpacing = 4;
const int kTitleCloseSpacing = 4;
const SkScalar kRoundRectRadius = 5;
const SkColor kTabBackgroundColor = SK_ColorWHITE;
const SkAlpha kBackgroundTabAlpha = 170;
};

////////////////////////////////////////////////////////////////////////////////
// SideTab, public:

SideTab::SideTab(TabController* controller)
    : BaseTab(controller) {
}

SideTab::~SideTab() {
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, views::View overrides:

void SideTab::Layout() {
  int icon_y;
  int icon_x = icon_y = (height() - kIconSize) / 2;
  // TODO(sky): big mini icons.
  icon_bounds_.SetRect(icon_x, icon_y, kIconSize, kIconSize);

  gfx::Size ps = close_button()->GetPreferredSize();
  int close_y = (height() - ps.height()) / 2;
  close_button()->SetBounds(
      std::max(0, width() - ps.width() - close_y),
      close_y,
      ps.width(),
      ps.height());

  int title_y = (height() - font_height()) / 2;
  int title_x = icon_bounds_.right() + kIconTitleSpacing;
  title_bounds_.SetRect(
      title_x,
      title_y,
      std::max(0, close_button()->x() - kTitleCloseSpacing - title_x),
      font_height());
}

void SideTab::Paint(gfx::Canvas* canvas) {
  SkPaint paint;
  paint.setColor(kTabBackgroundColor);
  paint.setAntiAlias(true);
  gfx::Path tab_shape;
  FillTabShapePath(&tab_shape);
  canvas->drawPath(tab_shape, paint);

  PaintIcon(canvas, icon_bounds_.x(), icon_bounds_.y());
  PaintTitle(canvas, SK_ColorBLACK);

  if (!IsSelected() && GetThemeProvider()->ShouldUseNativeFrame()) {
    // Make sure un-selected tabs are somewhat transparent.
    SkPaint paint;

    SkAlpha opacity = kBackgroundTabAlpha;
    if (hover_animation() && hover_animation()->is_animating()) {
      opacity =
          static_cast<SkAlpha>(hover_animation()->GetCurrentValue() * 255);
    }

    paint.setColor(SkColorSetARGB(kBackgroundTabAlpha, 255, 255, 255));
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    canvas->FillRectInt(0, 0, width(), height(), paint);
  }
}

gfx::Size SideTab::GetPreferredSize() {
  return gfx::Size(0, 27);
}

////////////////////////////////////////////////////////////////////////////////
// SideTab, private:

#define CUBIC_ARC_FACTOR    ((SK_ScalarSqrt2 - SK_Scalar1) * 4 / 3)

void SideTab::FillTabShapePath(gfx::Path* path) {
  SkScalar s = SkScalarMul(kRoundRectRadius, CUBIC_ARC_FACTOR);
  path->moveTo(SkIntToScalar(kRoundRectRadius), 0);
  path->cubicTo(SkIntToScalar(kRoundRectRadius) - s, 0, 0,
                SkIntToScalar(kRoundRectRadius) - s, 0,
                SkIntToScalar(kRoundRectRadius));
  path->lineTo(0, SkIntToScalar(height() - kRoundRectRadius));
  path->cubicTo(0, SkIntToScalar(height() - kRoundRectRadius) + s,
                SkIntToScalar(kRoundRectRadius) - s, SkIntToScalar(height()),
                SkIntToScalar(kRoundRectRadius),
                SkIntToScalar(height()));
  path->lineTo(SkIntToScalar(width()), SkIntToScalar(height()));
  path->lineTo(SkIntToScalar(width()), 0);
  path->lineTo(SkIntToScalar(kRoundRectRadius), 0);
  path->close();
}
