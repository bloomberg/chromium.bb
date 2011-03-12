// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/infobar_arrow_model.h"

#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_utils_gtk.h"

const size_t InfoBarArrowModel::kDefaultArrowSize = 12;

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
                              const gfx::Rect& bounds,
                              const GdkColor& border_color) {
  if (!NeedToDrawInfoBarArrow())
    return;

  SkPath path;
  path.moveTo(bounds.x() + 0.5, bounds.bottom() + 0.5);
  path.rLineTo(bounds.width() / 2.0, -bounds.height());
  path.lineTo(bounds.right() + 0.5, bounds.bottom() + 0.5);
  path.close();

  SkPaint paint;
  paint.setStrokeWidth(1);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(bounds.bottom()));
  grad_points[1].set(SkIntToScalar(0),
                     SkIntToScalar(bounds.bottom() + InfoBar::kInfoBarHeight));

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
