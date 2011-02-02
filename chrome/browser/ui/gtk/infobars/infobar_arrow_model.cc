// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/infobar_arrow_model.h"

#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "gfx/canvas_skia_paint.h"
#include "gfx/color_utils.h"
#include "gfx/point.h"
#include "gfx/skia_utils_gtk.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

InfoBarArrowModel::InfoBarArrowModel(Observer* observer)
    : observer_(observer),
      animation_(this) {
  animation_.SetTweenType(ui::Tween::LINEAR);
  animation_.Reset(1.0);
  target_colors_.top = target_colors_.bottom = SkColorSetARGB(0, 0, 0, 0);
  previous_colors_ = target_colors_;
}

InfoBarArrowModel::~InfoBarArrowModel() {
}

InfoBarArrowModel::InfoBarColors InfoBarArrowModel::CurrentInfoBarColors() {
  double alpha = animation_.GetCurrentValue();
  InfoBarColors colors = {
      color_utils::AlphaBlend(target_colors_.top,
                              previous_colors_.top,
                              alpha * 0xff),
      color_utils::AlphaBlend(target_colors_.bottom,
                              previous_colors_.bottom,
                              alpha * 0xff)};
  return colors;
}

bool InfoBarArrowModel::NeedToDrawInfoBarArrow() {
  return SkColorGetA(CurrentInfoBarColors().top) != 0;
}

void InfoBarArrowModel::ShowArrowFor(InfoBar* bar, bool animate) {
  scoped_ptr<std::pair<SkColor, SkColor> > colors;

  previous_colors_ = CurrentInfoBarColors();

  if (bar) {
    double r, g, b;
    bar->GetTopColor(bar->delegate()->GetInfoBarType(), &r, &g, &b);
    target_colors_.top = SkColorSetRGB(r * 0xff, g * 0xff, b * 0xff);
    bar->GetBottomColor(bar->delegate()->GetInfoBarType(), &r, &g, &b);
    target_colors_.bottom = SkColorSetRGB(r * 0xff, g * 0xff, b * 0xff);
  } else {
    target_colors_.bottom = target_colors_.top = SkColorSetARGB(0, 0, 0, 0);
  }

  if (animate) {
    // Fade from the current color to the target color.
    animation_.Reset();
    animation_.Show();
  } else {
    // Skip straight to showing the target color.
    animation_.Reset(1.0);
  }

  observer_->PaintStateChanged();
}

void InfoBarArrowModel::Paint(GtkWidget* widget,
                              GdkEventExpose* expose,
                              const gfx::Point& origin,
                              const GdkColor& border_color) {
  if (!NeedToDrawInfoBarArrow())
    return;

  // The size of the arrow (its height; also half its width).
  const int kArrowSize = 10;

  SkPath path;
  path.moveTo(SkPoint::Make(origin.x() - kArrowSize, origin.y()));
  path.rLineTo(kArrowSize, -kArrowSize);
  path.rLineTo(kArrowSize, kArrowSize);
  path.close();

  SkPaint paint;
  paint.setStrokeWidth(1);
  paint.setStyle(SkPaint::kFill_Style);

  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(origin.y()));
  grad_points[1].set(SkIntToScalar(0),
                     SkIntToScalar(origin.y() + InfoBar::kInfoBarHeight));

  InfoBarColors colors = CurrentInfoBarColors();
  SkColor grad_colors[2];
  grad_colors[0] = colors.top;
  grad_colors[1] = colors.bottom;

  SkShader* gradient_shader = SkGradientShader::CreateLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kMirror_TileMode);
  paint.setShader(gradient_shader);
  gradient_shader->unref();

  gfx::CanvasSkiaPaint canvas(expose, false);
  canvas.drawPath(path, paint);

  paint.setShader(NULL);
  paint.setColor(SkColorSetA(gfx::GdkColorToSkColor(border_color),
                             SkColorGetA(colors.top)));
  paint.setStyle(SkPaint::kStroke_Style);
  canvas.drawPath(path, paint);
}

void InfoBarArrowModel::AnimationEnded(const ui::Animation* animation) {
  observer_->PaintStateChanged();
}

void InfoBarArrowModel::AnimationProgressed(const ui::Animation* animation) {
  observer_->PaintStateChanged();
}

void InfoBarArrowModel::AnimationCanceled(const ui::Animation* animation) {
  observer_->PaintStateChanged();
}
