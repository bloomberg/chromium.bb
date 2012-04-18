// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/rounded_rect_painter.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace chromeos {

namespace {

const SkColor kScreenTopColor = SkColorSetRGB(250, 251, 251);
const SkColor kScreenBottomColor = SkColorSetRGB(204, 209, 212);
const SkColor kScreenShadowColor = SkColorSetARGB(64, 34, 54, 115);
const int kScreenShadow = 10;

void DrawRoundedRect(gfx::Canvas* canvas,
                     const SkRect& rect,
                     int corner_radius,
                     SkColor top_color,
                     SkColor bottom_color) {
  SkPath path;
  path.addRoundRect(rect,
                    SkIntToScalar(corner_radius), SkIntToScalar(corner_radius));
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  if (top_color != bottom_color) {
    SkPoint p[2];
    p[0].set(SkIntToScalar(rect.left()), SkIntToScalar(rect.top()));
    p[1].set(SkIntToScalar(rect.left()), SkIntToScalar(rect.bottom()));
    SkColor colors[2] = { top_color, bottom_color };
    SkShader* s = SkGradientShader::CreateLinear(p, colors, NULL, 2,
        SkShader::kClamp_TileMode, NULL);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();
  } else {
    paint.setColor(top_color);
  }

  canvas->DrawPath(path, paint);
}

void DrawRoundedRectShadow(gfx::Canvas* canvas,
                           const SkRect& rect,
                           int corner_radius,
                           int shadow,
                           SkColor color) {
  SkPaint paint;
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(color);
  SkMaskFilter* filter = SkBlurMaskFilter::Create(
      shadow / 2, SkBlurMaskFilter::kNormal_BlurStyle);
  paint.setMaskFilter(filter)->unref();
  SkRect inset_rect(rect);
  inset_rect.inset(SkIntToScalar(shadow / 2), SkIntToScalar(shadow / 2));
  canvas->sk_canvas()->drawRoundRect(
      inset_rect,
      SkIntToScalar(corner_radius), SkIntToScalar(corner_radius),
      paint);
  paint.setMaskFilter(NULL);
}

void DrawRectWithBorder(gfx::Canvas* canvas,
                        const gfx::Size& size,
                        const BorderDefinition* const border) {
  SkRect rect(gfx::RectToSkRect(gfx::Rect(size)));
  if (border->padding > 0) {
    SkPaint paint;
    paint.setColor(border->padding_color);
    canvas->sk_canvas()->drawRect(rect, paint);
    rect.inset(SkIntToScalar(border->padding), SkIntToScalar(border->padding));
  }
  if (border->shadow > 0) {
    DrawRoundedRectShadow(canvas, rect, border->corner_radius, border->shadow,
                          border->shadow_color);
    rect.inset(SkIntToScalar(border->shadow), SkIntToScalar(border->shadow));
    rect.offset(0, SkIntToScalar(-border->shadow / 3));
  }
  DrawRoundedRect(canvas, rect, border->corner_radius, border->top_color,
                  border->bottom_color);
}

// This Painter can be used to draw a background consistent cross all login
// screens. It draws a rect with padding, shadow and rounded corners.
class RoundedRectPainter : public views::Painter {
 public:
  explicit RoundedRectPainter(const BorderDefinition* const border)
      : border_(border) {
  }

  // Overridden from views::Painter:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE {
    DrawRectWithBorder(canvas, size, border_);
  }

 private:
  const BorderDefinition* const border_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectPainter);
};

// This border can be used to draw shadow and rounded corners across all
// login screens.
class RoundedRectBorder : public views::Border {
 public:
  explicit RoundedRectBorder(const BorderDefinition* const border)
      : border_(border) {
  }

  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const;
  virtual void GetInsets(gfx::Insets* insets) const;

 private:
  const BorderDefinition* const border_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectBorder);
};

void RoundedRectBorder::Paint(const views::View& view,
                              gfx::Canvas* canvas) const {
  // Don't paint anything. RoundedRectBorder is used to provide insets only.
}

void RoundedRectBorder::GetInsets(gfx::Insets* insets) const {
  DCHECK(insets);
  int shadow = border_->shadow;
  int inset = border_->corner_radius / 2 + border_->padding + shadow;
  insets->Set(inset - shadow / 3, inset, inset + shadow / 3, inset);
}

// Simple solid round background.
class RoundedBackground : public views::Background {
 public:
  RoundedBackground(int corner_radius,
                    int stroke_width,
                    SkColor background_color,
                    SkColor stroke_color)
      : corner_radius_(corner_radius),
        stroke_width_(stroke_width),
        stroke_color_(stroke_color) {
    SetNativeControlColor(background_color);
  }

  virtual void Paint(gfx::Canvas* canvas, views::View* view) const {
    SkRect rect;
    rect.iset(0, 0, view->width(), view->height());
    SkPath path;
    path.addRoundRect(rect,
                      SkIntToScalar(corner_radius_),
                      SkIntToScalar(corner_radius_));
    // Draw interior.
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(get_color());
    canvas->DrawPath(path, paint);
    // Redraw boundary region with correspoinding color.
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(stroke_width_));
    paint.setColor(stroke_color_);
    canvas->DrawPath(path, paint);
  }

 private:
  int corner_radius_;
  int stroke_width_;
  SkColor stroke_color_;

  DISALLOW_COPY_AND_ASSIGN(RoundedBackground);
};

}  // namespace

// static
const BorderDefinition BorderDefinition::kScreenBorder = {
  0,
  SK_ColorBLACK,
  kScreenShadow,
  kScreenShadowColor,
  login::kScreenCornerRadius,
  kScreenTopColor,
  kScreenBottomColor
};

const BorderDefinition BorderDefinition::kUserBorder = {
  0,
  SK_ColorBLACK,
  0,
  kScreenShadowColor,
  login::kUserCornerRadius,
  kScreenTopColor,
  kScreenBottomColor
};

views::Painter* CreateWizardPainter(const BorderDefinition* const border) {
  return new RoundedRectPainter(border);
}

views::Border* CreateWizardBorder(const BorderDefinition* const border) {
  return new RoundedRectBorder(border);
}

views::Background* CreateRoundedBackground(int corner_radius,
                                           int stroke_width,
                                           SkColor background_color,
                                           SkColor stroke_color) {
  return new RoundedBackground(
      corner_radius, stroke_width, background_color, stroke_color);
}

}  // namespace chromeos
