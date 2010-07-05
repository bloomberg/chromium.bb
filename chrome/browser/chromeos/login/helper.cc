// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"
#include "gfx/canvas_skia.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/throbber.h"
#include "views/painter.h"
#include "views/screen.h"

namespace chromeos {

namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

// Time in ms before smothed throbber is shown.
const int kThrobberStartDelayMs = 500;

const SkColor kBackgroundCenterColor = SkColorSetRGB(41, 50, 67);
const SkColor kBackgroundEdgeColor = SK_ColorBLACK;

class BackgroundPainter : public views::Painter {
 public:
  BackgroundPainter() {}

  virtual void Paint(int w, int h, gfx::Canvas* canvas) {
    SkRect rect;
    rect.set(SkIntToScalar(0),
             SkIntToScalar(0),
             SkIntToScalar(w),
             SkIntToScalar(h));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    SkColor colors[2] = { kBackgroundCenterColor, kBackgroundEdgeColor };
    SkShader* s = SkGradientShader::CreateRadial(
        SkPoint::Make(SkIntToScalar(w / 2), SkIntToScalar(h / 2)),
        SkIntToScalar(w / 2),
        colors,
        NULL,
        2,
        SkShader::kClamp_TileMode,
        NULL);
    paint.setShader(s);
    s->unref();
    canvas->AsCanvasSkia()->drawRect(rect, paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundPainter);
};

}  // namespace

views::Throbber* CreateDefaultSmoothedThrobber() {
  views::SmoothedThrobber* throbber =
      new views::SmoothedThrobber(kThrobberFrameMs);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  throbber->set_start_delay_ms(kThrobberStartDelayMs);
  return throbber;
}

views::Throbber* CreateDefaultThrobber() {
  views::Throbber* throbber = new views::Throbber(kThrobberFrameMs, false);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  return throbber;
}

views::Painter* CreateBackgroundPainter() {
  return new BackgroundPainter();
}

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds(views::Screen::GetMonitorWorkAreaNearestWindow(NULL));
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }

  return bounds;
}

}  // namespace chromeos

