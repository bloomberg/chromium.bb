// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/rounded_rect_painter.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace chromeos {

namespace {

const SkColor kScreenTopColor = SkColorSetRGB(250, 251, 251);
const SkColor kScreenBottomColor = SkColorSetRGB(204, 209, 212);
const SkColor kScreenShadowColor = SkColorSetARGB(64, 34, 54, 115);
const SkColor kShadowStrokeColor = 0;
const int kScreenShadow = 10;

static void DrawRoundedRect(
      gfx::Canvas* canvas,
      int x, int y,
      int w, int h,
      int corner_radius,
      SkColor top_color, SkColor bottom_color,
      SkColor stroke_color) {
  SkRect rect;
  rect.set(
      SkIntToScalar(x), SkIntToScalar(y),
      SkIntToScalar(x + w), SkIntToScalar(y + h));
  SkPath path;
  path.addRoundRect(
      rect, SkIntToScalar(corner_radius), SkIntToScalar(corner_radius));
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  if (top_color != bottom_color) {
    SkPoint p[2];
    p[0].set(SkIntToScalar(x), SkIntToScalar(y));
    p[1].set(SkIntToScalar(x), SkIntToScalar(y + h));
    SkColor colors[2] = { top_color, bottom_color };
    SkShader* s =
        SkGradientShader::CreateLinear(p, colors, NULL, 2,
                                       SkShader::kClamp_TileMode, NULL);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();
  } else {
    paint.setColor(top_color);
  }
  canvas->GetSkCanvas()->drawPath(path, paint);

  if (stroke_color != 0) {
    // Expand rect by 0.5px so resulting stroke will take the whole pixel.
    rect.set(
        SkIntToScalar(x) - SK_ScalarHalf,
        SkIntToScalar(y) - SK_ScalarHalf,
        SkIntToScalar(x + w) + SK_ScalarHalf,
        SkIntToScalar(y + h) + SK_ScalarHalf);
    paint.setShader(NULL);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(SK_Scalar1));
    paint.setColor(stroke_color);
    canvas->GetSkCanvas()->drawRoundRect(
      rect,
      SkIntToScalar(corner_radius), SkIntToScalar(corner_radius),
      paint);
  }
}

static void DrawRoundedRectShadow(
    gfx::Canvas* canvas,
    int x, int y,
    int w, int h,
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
  SkRect rect;
  rect.set(
      SkIntToScalar(x + shadow / 2), SkIntToScalar(y + shadow / 2),
      SkIntToScalar(x + w - shadow / 2), SkIntToScalar(y + h - shadow / 2));
  canvas->GetSkCanvas()->drawRoundRect(
      rect,
      SkIntToScalar(corner_radius), SkIntToScalar(corner_radius),
      paint);
  paint.setMaskFilter(NULL);
}

static void DrawRectWithBorder(int w,
                               int h,
                               const BorderDefinition* const border,
                               gfx::Canvas* canvas) {
  int padding = border->padding;
  SkColor padding_color = border->padding_color;
  int shadow = border->shadow;
  SkColor shadow_color = border->shadow_color;
  int corner_radius = border->corner_radius;
  SkColor top_color = border->top_color;
  SkColor bottom_color = border->bottom_color;
  if (padding > 0) {
    SkPaint paint;
    paint.setColor(padding_color);
    canvas->GetSkCanvas()->drawRectCoords(
        SkIntToScalar(0), SkIntToScalar(0), SkIntToScalar(w), SkIntToScalar(h),
        paint);
  }
  if (border->shadow > 0) {
    DrawRoundedRectShadow(
        canvas,
        padding, padding,
        w - 2 * padding, h - 2 * padding,
        corner_radius,
        shadow, shadow_color);
  }
  DrawRoundedRect(
        canvas,
        padding + shadow,
        padding + shadow - shadow / 3,
        w - 2 * padding - 2 * shadow,
        h - 2 * padding - 2 * shadow,
        corner_radius,
        top_color, bottom_color,
        shadow ? kShadowStrokeColor : 0);
}

// This Painter can be used to draw a background consistent cross all login
// screens. It draws a rect with padding, shadow and rounded corners.
class RoundedRectPainter : public views::Painter {
 public:
  explicit RoundedRectPainter(const BorderDefinition* const border)
      : border_(border) {
  }

  virtual void Paint(int w, int h, gfx::Canvas* canvas) {
    DrawRectWithBorder(w, h, border_, canvas);
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
                    const SkColor& background_color,
                    const SkColor& stroke_color)
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
    canvas->GetSkCanvas()->drawPath(path, paint);
    // Redraw boundary region with correspoinding color.
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(stroke_width_));
    paint.setColor(stroke_color_);
    canvas->GetSkCanvas()->drawPath(path, paint);
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
