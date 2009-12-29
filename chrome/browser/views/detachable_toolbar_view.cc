// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/detachable_toolbar_view.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser_theme_provider.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkShader.h"

// How round the 'new tab' style bookmarks bar is.
static const int kNewtabBarRoundness = 5;

const SkColor DetachableToolbarView::kEdgeDividerColor =
    SkColorSetRGB(222, 234, 248);
const SkColor DetachableToolbarView::kMiddleDividerColor =
    SkColorSetRGB(194, 205, 212);

// static
void DetachableToolbarView::PaintBackgroundAttachedMode(gfx::Canvas* canvas,
                                                        views::View* view) {
  gfx::Rect bounds =
      view->GetBounds(views::View::APPLY_MIRRORING_TRANSFORMATION);

  ThemeProvider* tp = view->GetThemeProvider();
  SkColor theme_toolbar_color =
      tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);
  canvas->FillRectInt(theme_toolbar_color, 0, 0,
                      view->width(), view->height());

  canvas->TileImageInt(*tp->GetBitmapNamed(IDR_THEME_TOOLBAR),
      view->GetParent()->GetBounds(
      views::View::APPLY_MIRRORING_TRANSFORMATION).x() + bounds.x(),
      bounds.y(), 0, 0, view->width(), view->height());
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
  // the view (bar/shelf) is at the top/at the bottom and whether it is attached
  // or detached.
  int y = view->IsOnTop() == !view->IsDetached() ? view->height() - 1 : 0;
  canvas->FillRectInt(ResourceBundle::toolbar_separator_color,
      0, y, view->width(), 1);
}

// static
void DetachableToolbarView::PaintContentAreaBackground(
    gfx::Canvas* canvas, ThemeProvider* theme_provider,
    const SkRect& rect, double roundness) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(theme_provider->GetColor(BrowserThemeProvider::COLOR_TOOLBAR));

  canvas->drawRoundRect(
      rect, SkDoubleToScalar(roundness), SkDoubleToScalar(roundness), paint);
}

// static
void DetachableToolbarView::PaintContentAreaBorder(
    gfx::Canvas* canvas, ThemeProvider* theme_provider,
    const SkRect& rect, double roundness) {
  SkPaint border_paint;
  border_paint.setColor(
      theme_provider->GetColor(BrowserThemeProvider::COLOR_NTP_HEADER));
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setAlpha(96);
  border_paint.setAntiAlias(true);

  canvas->drawRoundRect(rect,
                        SkDoubleToScalar(roundness),
                        SkDoubleToScalar(roundness), border_paint);
}

// static
void DetachableToolbarView::PaintVerticalDivider(
    gfx::Canvas* canvas, int x, int height, int vertical_padding,
    const SkColor& top_color,
    const SkColor& middle_color,
    const SkColor& bottom_color) {
  // Draw the upper half of the divider.
  SkPaint paint;
  paint.setShader(skia::CreateGradientShader(vertical_padding + 1,
                                             height / 2,
                                             top_color,
                                             middle_color))->safeUnref();
  SkRect rc = { SkIntToScalar(x),
                SkIntToScalar(vertical_padding + 1),
                SkIntToScalar(x + 1),
                SkIntToScalar(height / 2) };
  canvas->drawRect(rc, paint);

  // Draw the lower half of the divider.
  SkPaint paint_down;
  paint_down.setShader(skia::CreateGradientShader(height / 2,
                                                  height - vertical_padding,
                                                  middle_color,
                                                  bottom_color))->safeUnref();
  SkRect rc_down = { SkIntToScalar(x),
                     SkIntToScalar(height / 2),
                     SkIntToScalar(x + 1),
                     SkIntToScalar(height - vertical_padding) };
  canvas->drawRect(rc_down, paint_down);
}
