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
  gfx::ScopedCanvas scoped(canvas);
  // Undo the scale factor so we can stroke with a width of 1px (not 1dp).
  const float dsf = canvas->UndoDeviceScaleFactor();
  // Always scale to a whole number to make sure strokes are sharp.
  // For some fractional DSFs, view size rounding issues may cause there to be a
  // gap above the infobar (below the horizontal stroke painted by the previous
  // infobar or the toolbar). We use floor here to "pull" the fill region up a
  // pixel and bridge the gap. Sometimes this will be 1px too high, so we also
  // re-draw the horizontal portions of the top separator in case this floor
  // operation has covered it up. For whole-number DSFs, the floor should have
  // no effect.
  auto scale = [dsf](int dimension) { return std::floor(dimension * dsf); };
  SkScalar arrow_height = scale(infobar->arrow_height());
  SkScalar infobar_width = scale(infobar->width());
  if (delegate) {
    separator_color = delegate->GetInfoBarSeparatorColor();
    int arrow_x;
    if (delegate->DrawInfoBarArrows(&arrow_x) && infobar->arrow_height() > 0) {
      // Compensate for the fact that a relative movement of n creates a line of
      // length n + 1 once the path is placed on pixel centers and stroked.
      SkScalar r_arrow_half_width = scale(infobar->arrow_half_width()) - 1;
      SkScalar r_arrow_height = arrow_height - 1;
      SkScalar arrow_tip_x = scale(arrow_x);
      stroke_path.moveTo(0, r_arrow_height);
      stroke_path.lineTo(arrow_tip_x - r_arrow_half_width, r_arrow_height);
      stroke_path.lineTo(arrow_tip_x, 0);
      stroke_path.lineTo(arrow_tip_x + r_arrow_half_width, r_arrow_height);
      stroke_path.lineTo(infobar_width - 1, r_arrow_height);

      // Add SK_ScalarHalf to both axes to place the path on pixel centers.
      stroke_path.offset(SK_ScalarHalf, SK_ScalarHalf);

      fill_path = stroke_path;
      fill_path.close();
    }
  }
  fill_path.addRect(0, arrow_height, infobar_width, scale(infobar->height()));
  SkPaint fill;
  fill.setStyle(SkPaint::kFill_Style);
  fill.setColor(top_color_);
  canvas->DrawPath(fill_path, fill);

  SkPaint stroke;
  stroke.setStyle(SkPaint::kStroke_Style);
  const int kSeparatorThicknessPx = 1;
  stroke.setStrokeWidth(SkIntToScalar(kSeparatorThicknessPx));
  stroke.setColor(separator_color);
  stroke.setAntiAlias(true);
  canvas->DrawPath(stroke_path, stroke);

  // Bottom separator.
  stroke.setAntiAlias(false);
  gfx::SizeF view_size_px = gfx::ScaleSize(gfx::SizeF(view->size()), dsf);
  SkScalar y = SkIntToScalar(view_size_px.height() - kSeparatorThicknessPx) +
               SK_ScalarHalf;
  SkScalar w = SkFloatToScalar(view_size_px.width());
  canvas->sk_canvas()->drawLine(0, y, w, y, stroke);
}
