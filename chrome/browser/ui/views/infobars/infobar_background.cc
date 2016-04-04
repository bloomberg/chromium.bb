// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_background.h"

#include "chrome/browser/ui/infobar_container_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "components/infobars/core/infobar.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

InfoBarBackground::InfoBarBackground(
    infobars::InfoBarDelegate::Type infobar_type)
    : top_color_(infobars::InfoBar::GetTopColor(infobar_type)),
      bottom_color_(infobars::InfoBar::GetBottomColor(infobar_type)) {
  SetNativeControlColor(
      color_utils::AlphaBlend(top_color_, bottom_color_, 128));
}

InfoBarBackground::~InfoBarBackground() {
}

void InfoBarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    PaintMd(canvas, view);
    return;
  }

  SkPoint gradient_points[2];
  gradient_points[0].iset(0, 0);
  gradient_points[1].iset(0, view->height());
  SkColor gradient_colors[2] = {top_color_, bottom_color_};
  SkPaint paint;
  paint.setStrokeWidth(
      SkIntToScalar(InfoBarContainerDelegate::kSeparatorLineHeight));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setShader(SkGradientShader::MakeLinear(
      gradient_points, gradient_colors, nullptr, 2, SkShader::kClamp_TileMode));

  SkCanvas* canvas_skia = canvas->sk_canvas();
  InfoBarView* infobar = static_cast<InfoBarView*>(view);
  canvas_skia->drawPath(infobar->fill_path(), paint);

  paint.setShader(nullptr);
  SkColor separator_color =
      infobar->container_delegate()->GetInfoBarSeparatorColor();
  paint.setColor(SkColorSetA(separator_color, SkColorGetA(gradient_colors[0])));
  paint.setStyle(SkPaint::kStroke_Style);
  // Anti-alias the path so it doesn't look goofy when the edges are not at 45
  // degree angles, but don't anti-alias anything else, especially not the fill,
  // lest we get weird color bleeding problems.
  paint.setAntiAlias(true);
  canvas_skia->drawPath(infobar->stroke_path(), paint);
  paint.setAntiAlias(false);

  // Now draw the separator at the bottom.
  canvas->FillRect(
      gfx::Rect(0,
                view->height() - InfoBarContainerDelegate::kSeparatorLineHeight,
                view->width(), InfoBarContainerDelegate::kSeparatorLineHeight),
      separator_color);
}

void InfoBarBackground::PaintMd(gfx::Canvas* canvas, views::View* view) const {
  InfoBarView* infobar = static_cast<InfoBarView*>(view);
  const infobars::InfoBarContainer::Delegate* delegate =
      infobar->container_delegate();
  SkPath stroke_path, fill_path;
  SkColor separator_color = SK_ColorBLACK;
  if (delegate) {
    separator_color = delegate->GetInfoBarSeparatorColor();
    int arrow_x;
    if (delegate->DrawInfoBarArrows(&arrow_x) && infobar->arrow_height() > 0) {
      SkScalar arrow_fill_height = SkIntToScalar(infobar->arrow_height());
      SkScalar arrow_fill_half_width =
          SkIntToScalar(infobar->arrow_half_width());
      stroke_path.moveTo(SkIntToScalar(arrow_x) - arrow_fill_half_width,
                         SkIntToScalar(infobar->arrow_height()));
      stroke_path.rLineTo(arrow_fill_half_width, -arrow_fill_height);
      stroke_path.rLineTo(arrow_fill_half_width, arrow_fill_height);

      fill_path = stroke_path;
      fill_path.close();
    }
  }
  fill_path.addRect(0, SkIntToScalar(infobar->arrow_height()),
                    SkIntToScalar(infobar->width()),
                    SkIntToScalar(infobar->height()));

  gfx::ScopedCanvas scoped(canvas);
  // Undo the scale factor so we can stroke with a width of 1px (not 1dp).
  const float scale = canvas->UndoDeviceScaleFactor();
  // View bounds are in dp. Manually scale for px.
  gfx::SizeF view_size_px = gfx::ScaleSize(gfx::SizeF(view->size()), scale);

  SkPaint fill;
  fill.setStyle(SkPaint::kFill_Style);
  fill.setColor(top_color_);

  SkCanvas* canvas_skia = canvas->sk_canvas();
  // The paths provided by the above calculations are in dp. Manually scale for
  // px.
  SkMatrix dsf_transform;
  dsf_transform.setScale(SkFloatToScalar(scale), SkFloatToScalar(scale));
  fill_path.transform(dsf_transform);
  // In order to affect exactly the pixels we want, the fill and stroke paths
  // need to go through the pixel centers instead of along the pixel
  // edges/corners. Skia considers integral coordinates to be the edges between
  // pixels, so offset by 0.5 to get to the centers.
  fill_path.offset(SK_ScalarHalf, SK_ScalarHalf);
  canvas_skia->drawPath(fill_path, fill);

  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  const int kSeparatorThicknessPx = 1;
  stroke.setStrokeWidth(SkIntToScalar(kSeparatorThicknessPx));
  stroke.setColor(separator_color);
  stroke_path.transform(dsf_transform);
  stroke_path.offset(SK_ScalarHalf, SK_ScalarHalf);

  // The arrow part of the top separator. (The rest was drawn by the toolbar or
  // the infobar above this one.)
  stroke.setAntiAlias(true);
  canvas_skia->drawPath(stroke_path, stroke);
  // Bottom separator.
  stroke.setAntiAlias(false);
  SkScalar y = SkIntToScalar(view_size_px.height() - kSeparatorThicknessPx) +
               SK_ScalarHalf;
  SkScalar w = SkIntToScalar(view_size_px.width());
  canvas_skia->drawLine(0, y, w, y, stroke);
}
