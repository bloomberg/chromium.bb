// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/detachable_toolbar_view.h"

#include "chrome/browser/themes/theme_service.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skia_util.h"
#include "views/window/non_client_view.h"

// How round the 'new tab' style bookmarks bar is.
static const int kNewtabBarRoundness = 5;

const SkColor DetachableToolbarView::kEdgeDividerColor =
    SkColorSetRGB(222, 234, 248);
const SkColor DetachableToolbarView::kMiddleDividerColor =
    SkColorSetRGB(194, 205, 212);

// static
void DetachableToolbarView::PaintBackgroundAttachedMode(
    gfx::Canvas* canvas,
    views::View* view,
    const gfx::Point& background_origin) {
  ui::ThemeProvider* tp = view->GetThemeProvider();
  SkColor theme_toolbar_color =
      tp->GetColor(ThemeService::COLOR_TOOLBAR);
  canvas->FillRectInt(theme_toolbar_color, 0, 0,
                      view->width(), view->height());
  canvas->TileImageInt(*tp->GetBitmapNamed(IDR_THEME_TOOLBAR),
                       background_origin.x(), background_origin.y(), 0, 0,
                       view->width(), view->height());
}

// static
void DetachableToolbarView::CalculateContentArea(
    double animation_state, double horizontal_padding,
    double vertical_padding, SkRect* rect,
    double* roundness, views::View* view) {
  // The 0.5 is to correct for Skia's "draw on pixel boundaries"ness.
  rect->set(SkDoubleToScalar(horizontal_padding - 0.5),
           SkDoubleToScalar(vertical_padding - 0.5),
           SkDoubleToScalar(view->width() - horizontal_padding - 0.5),
           SkDoubleToScalar(view->height() - vertical_padding - 0.5));

  *roundness = static_cast<double>(kNewtabBarRoundness) * animation_state;
}

// static
void DetachableToolbarView::PaintHorizontalBorder(gfx::Canvas* canvas,
                                                  DetachableToolbarView* view) {
  // Border can be at the top or at the bottom of the view depending on whether
  // the view (bar/shelf) is attached or detached.
  int thickness = views::NonClientFrameView::kClientEdgeThickness;
  int y = view->IsDetached() ? 0 : (view->height() - thickness);
  canvas->FillRectInt(ResourceBundle::toolbar_separator_color,
      0, y, view->width(), thickness);
}

// static
void DetachableToolbarView::PaintContentAreaBackground(
    gfx::Canvas* canvas, ui::ThemeProvider* theme_provider,
    const SkRect& rect, double roundness) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(theme_provider->GetColor(ThemeService::COLOR_TOOLBAR));

  canvas->AsCanvasSkia()->drawRoundRect(
      rect, SkDoubleToScalar(roundness), SkDoubleToScalar(roundness), paint);
}

// static
void DetachableToolbarView::PaintContentAreaBorder(
    gfx::Canvas* canvas, ui::ThemeProvider* theme_provider,
    const SkRect& rect, double roundness) {
  SkPaint border_paint;
  border_paint.setColor(
      theme_provider->GetColor(ThemeService::COLOR_NTP_HEADER));
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setAlpha(96);
  border_paint.setAntiAlias(true);

  canvas->AsCanvasSkia()->drawRoundRect(
      rect, SkDoubleToScalar(roundness), SkDoubleToScalar(roundness),
      border_paint);
}

// static
void DetachableToolbarView::PaintVerticalDivider(
    gfx::Canvas* canvas, int x, int height, int vertical_padding,
    const SkColor& top_color,
    const SkColor& middle_color,
    const SkColor& bottom_color) {
  // Draw the upper half of the divider.
  SkPaint paint;
  SkSafeUnref(paint.setShader(gfx::CreateGradientShader(vertical_padding + 1,
                                                        height / 2,
                                                        top_color,
                                                        middle_color)));
  SkRect rc = { SkIntToScalar(x),
                SkIntToScalar(vertical_padding + 1),
                SkIntToScalar(x + 1),
                SkIntToScalar(height / 2) };
  canvas->AsCanvasSkia()->drawRect(rc, paint);

  // Draw the lower half of the divider.
  SkPaint paint_down;
  SkSafeUnref(paint_down.setShader(gfx::CreateGradientShader(
          height / 2, height - vertical_padding, middle_color, bottom_color)));
  SkRect rc_down = { SkIntToScalar(x),
                     SkIntToScalar(height / 2),
                     SkIntToScalar(x + 1),
                     SkIntToScalar(height - vertical_padding) };
  canvas->AsCanvasSkia()->drawRect(rc_down, paint_down);
}
