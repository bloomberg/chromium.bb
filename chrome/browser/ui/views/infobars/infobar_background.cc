// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_background.h"

#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/view.h"

// static
const int InfoBarBackground::kSeparatorLineHeight = 1;

InfoBarBackground::InfoBarBackground(InfoBarDelegate::Type infobar_type)
    : top_color_(GetTopColor(infobar_type)),
      bottom_color_(GetBottomColor(infobar_type)) {
}

InfoBarBackground::~InfoBarBackground() {
}

SkColor InfoBarBackground::GetTopColor(InfoBarDelegate::Type infobar_type) {
  static const SkColor kWarningBackgroundColorTop =
      SkColorSetRGB(255, 242, 183);
  static const SkColor kPageActionBackgroundColorTop =
      SkColorSetRGB(218, 231, 249);

  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorTop : kPageActionBackgroundColorTop;
}

SkColor InfoBarBackground::GetBottomColor(InfoBarDelegate::Type infobar_type) {
  static const SkColor kWarningBackgroundColorBottom =
      SkColorSetRGB(250, 230, 145);
  static const SkColor kPageActionBackgroundColorBottom =
      SkColorSetRGB(179, 202, 231);

  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorBottom : kPageActionBackgroundColorBottom;
}

void InfoBarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  SkPoint gradient_points[2] = {
    {SkIntToScalar(0), SkIntToScalar(0)},
    {SkIntToScalar(0), SkIntToScalar(view->height())}
  };
  SkColor gradient_colors[2] = {
    top_color_,
    bottom_color_
  };
  SkShader* gradient_shader = SkGradientShader::CreateLinear(
      gradient_points, gradient_colors, NULL, 2, SkShader::kClamp_TileMode);
  SkPaint paint;
  paint.setStrokeWidth(1.0);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setShader(gradient_shader);
  paint.setAntiAlias(true);
  gradient_shader->unref();

  InfoBarView* infobar = static_cast<InfoBarView*>(view);
  gfx::CanvasSkia* canvas_skia = canvas->AsCanvasSkia();
  canvas_skia->drawPath(*infobar->fill_path(), paint);

  paint.setShader(NULL);
  paint.setColor(SkColorSetA(ResourceBundle::toolbar_separator_color,
                             SkColorGetA(gradient_colors[0])));
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_skia->drawPath(*infobar->stroke_path(), paint);

  // Now draw at the bottom.
  canvas->FillRectInt(ResourceBundle::toolbar_separator_color, 0,
                      view->height() - kSeparatorLineHeight, view->width(),
                      kSeparatorLineHeight);
}
