// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/detachable_toolbar_view.h"

#include "app/gfx/canvas.h"
#include "chrome/browser/browser_theme_provider.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

// How round the 'new tab' style bookmarks bar is.
static const int kNewtabBarRoundness = 5;

const SkColor DetachableToolbarView::kEdgeDividerColor =
    SkColorSetRGB(222, 234, 248);
const SkColor DetachableToolbarView::kMiddleDividerColor =
    SkColorSetRGB(194, 205, 212);

// static
void DetachableToolbarView::PaintBackgroundDetachedMode(gfx::Canvas* canvas,
                                                        views::View* view) {
  int browser_height = view->GetParent()->GetBounds(
      views::View::APPLY_MIRRORING_TRANSFORMATION).height();

  // Draw the background to match the new tab page.
  ThemeProvider* tp = view->GetThemeProvider();
  canvas->FillRectInt(tp->GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND),
                      0, 0, view->width(), view->height());

  if (tp->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    int tiling = BrowserThemeProvider::NO_REPEAT;
    tp->GetDisplayProperty(BrowserThemeProvider::NTP_BACKGROUND_TILING,
                           &tiling);
    int alignment;
    if (tp->GetDisplayProperty(BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT,
        &alignment)) {
      SkBitmap* ntp_background = tp->GetBitmapNamed(IDR_THEME_NTP_BACKGROUND);

      if (alignment & BrowserThemeProvider::ALIGN_TOP) {
        PaintThemeBackgroundTopAligned(
            canvas, ntp_background, tiling, alignment,
            view->width(), view->height());
      } else {
        PaintThemeBackgroundBottomAligned(
            canvas, ntp_background, tiling, alignment,
            view->width(), view->height(), browser_height);
      }
    }
  }
}

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

// static
void DetachableToolbarView::PaintThemeBackgroundTopAligned(
    gfx::Canvas* canvas, SkBitmap* ntp_background, int tiling, int alignment,
    int width, int height) {
  if (alignment & BrowserThemeProvider::ALIGN_LEFT) {
    if (tiling == BrowserThemeProvider::REPEAT) {
      canvas->TileImageInt(*ntp_background, 0, 0, width, height);
    } else if (tiling == BrowserThemeProvider::REPEAT_X) {
      canvas->TileImageInt(*ntp_background, 0, 0, width,
                           ntp_background->height());
    } else {
      canvas->TileImageInt(*ntp_background, 0, 0,
                           ntp_background->width(), ntp_background->height());
    }
  } else if (alignment & BrowserThemeProvider::ALIGN_RIGHT) {
    int x_pos = width % ntp_background->width() - ntp_background->width();
    if (tiling == BrowserThemeProvider::REPEAT) {
      canvas->TileImageInt(*ntp_background, x_pos, 0,
                           width + ntp_background->width(), height);
    } else if (tiling == BrowserThemeProvider::REPEAT_X) {
      canvas->TileImageInt(*ntp_background, x_pos,
          0, width + ntp_background->width(), ntp_background->height());
    } else {
      canvas->TileImageInt(*ntp_background, width - ntp_background->width(),
          0, ntp_background->width(), ntp_background->height());
    }
  } else {  // ALIGN == CENTER
    int x_pos = width > ntp_background->width() ?
                ((width / 2 - ntp_background->width() / 2) %
                    ntp_background->width()) - ntp_background->width() :
                width / 2 - ntp_background->width() / 2;
    if (tiling == BrowserThemeProvider::REPEAT) {
      canvas->TileImageInt(*ntp_background, x_pos, 0,
                           width + ntp_background->width(), height);
    } else if (tiling == BrowserThemeProvider::REPEAT_X) {
      canvas->TileImageInt(*ntp_background, x_pos, 0,
                           width + ntp_background->width(),
                           ntp_background->height());
    } else {
     canvas->TileImageInt(*ntp_background,
                          width / 2 - ntp_background->width() / 2,
                          0, ntp_background->width(), ntp_background->height());
    }
  }
}

// static
void DetachableToolbarView::PaintThemeBackgroundBottomAligned(
    gfx::Canvas* canvas, SkBitmap* ntp_background, int tiling, int alignment,
    int width, int height, int browser_height) {
  int border_width = 5;
  int y_pos = ((tiling == BrowserThemeProvider::REPEAT_X) ||
              (tiling == BrowserThemeProvider::NO_REPEAT)) ?
        browser_height - ntp_background->height() - height - border_width :
  browser_height % ntp_background->height() - height - border_width -
    ntp_background->height();

  if (alignment & BrowserThemeProvider::ALIGN_LEFT) {
    if (tiling == BrowserThemeProvider::REPEAT) {
      canvas->TileImageInt(*ntp_background, 0, y_pos, width,
                           2 * height + ntp_background->height() + 5);
    } else if (tiling == BrowserThemeProvider::REPEAT_X) {
      canvas->TileImageInt(*ntp_background, 0, y_pos, width,
                           ntp_background->height());
    } else if (tiling == BrowserThemeProvider::REPEAT_Y) {
      canvas->TileImageInt(*ntp_background, 0, y_pos,
                           ntp_background->width(),
                           2 * height + ntp_background->height() + 5);
    } else {
      canvas->TileImageInt(*ntp_background, 0, y_pos, ntp_background->width(),
                           ntp_background->height());
    }
  } else if (alignment & BrowserThemeProvider::ALIGN_RIGHT) {
    int x_pos = width % ntp_background->width() - ntp_background->width();
    if (tiling == BrowserThemeProvider::REPEAT) {
      canvas->TileImageInt(*ntp_background, x_pos, y_pos,
                           width + ntp_background->width(),
                           2 * height + ntp_background->height() + 5);
    } else if (tiling == BrowserThemeProvider::REPEAT_X) {
      canvas->TileImageInt(*ntp_background, x_pos, y_pos,
          width + ntp_background->width(), ntp_background->height());
    } else if (tiling == BrowserThemeProvider::REPEAT_Y) {
      canvas->TileImageInt(*ntp_background, width - ntp_background->width(),
                             y_pos, ntp_background->width(),
                             2 * height + ntp_background->height() + 5);
    } else {
      canvas->TileImageInt(*ntp_background, width - ntp_background->width(),
          y_pos, ntp_background->width(), ntp_background->height());
    }
  } else {  // ALIGN == CENTER
    int x_pos = width > ntp_background->width() ?
                ((width / 2 - ntp_background->width() / 2) %
                    ntp_background->width()) - ntp_background->width() :
                width / 2 - ntp_background->width() / 2;
    if (tiling == BrowserThemeProvider::REPEAT) {
      canvas->TileImageInt(*ntp_background, x_pos, y_pos,
                           width + ntp_background->width(),
                           2 * height + ntp_background->height() + 5);
    } else if (tiling == BrowserThemeProvider::REPEAT_X) {
      canvas->TileImageInt(*ntp_background, x_pos, y_pos,
          width + ntp_background->width(), ntp_background->height());
    } else if (tiling == BrowserThemeProvider::REPEAT_Y) {
      canvas->TileImageInt(*ntp_background,
                           width / 2 - ntp_background->width() / 2,
                           y_pos, ntp_background->width(),
                           2 * height + ntp_background->height() + 5);
    } else {
      canvas->TileImageInt(*ntp_background,
          width / 2 - ntp_background->width() / 2,
          y_pos, ntp_background->width(), ntp_background->height());
    }
  }
}
