// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/windows_10_caption_button.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/scoped_canvas.h"

Windows10CaptionButton::Windows10CaptionButton(
    GlassBrowserFrameView* frame_view,
    ViewID button_type)
    : views::CustomButton(frame_view),
      frame_view_(frame_view),
      button_type_(button_type) {
  set_animate_on_state_change(true);
}

gfx::Size Windows10CaptionButton::GetPreferredSize() const {
  // TODO(bsep): The sizes in this function are for 1x device scale and don't
  // match Windows button sizes at hidpi.
  constexpr int kButtonHeightRestored = 29;
  int h = kButtonHeightRestored;
  if (frame_view_->IsMaximized()) {
    h = frame_view_->browser_view()->IsTabStripVisible()
            ? frame_view_->browser_view()->GetTabStripHeight()
            : frame_view_->TitlebarMaximizedVisualHeight();
    constexpr int kMaximizedBottomMargin = 2;
    h -= kMaximizedBottomMargin;
  }
  constexpr int kButtonWidth = 45;
  return gfx::Size(kButtonWidth, h);
}

void Windows10CaptionButton::OnPaint(gfx::Canvas* canvas) {
  PaintBackground(canvas);
  PaintSymbol(canvas);
}

void Windows10CaptionButton::PaintBackground(gfx::Canvas* canvas) {
  SkColor base_color;
  SkAlpha hovered_alpha, pressed_alpha;
  if (button_type_ == VIEW_ID_CLOSE_BUTTON) {
    base_color = SkColorSetRGB(0xE8, 0x11, 0x23);
    hovered_alpha = SK_AlphaOPAQUE;
    pressed_alpha = 0x99;
  } else {
    // TODO(bsep): These values are only correct for light themes.
    base_color = SK_ColorBLACK;
    hovered_alpha = 0x1B;
    pressed_alpha = 0x34;
  }
  SkAlpha alpha;
  if (state() == STATE_PRESSED)
    alpha = pressed_alpha;
  else
    alpha = gfx::Tween::IntValueBetween(hover_animation().GetCurrentValue(),
                                        SK_AlphaTRANSPARENT, hovered_alpha);
  canvas->FillRect(GetContentsBounds(), SkColorSetA(base_color, alpha));
}

namespace {

// Canvas::DrawRect's stroke can bleed out of |rect|'s bounds, so this draws a
// rectangle inset such that the result is constrained to |rect|'s size.
void DrawRect(gfx::Canvas* canvas,
              const gfx::Rect& rect,
              const SkPaint& paint) {
  gfx::RectF rect_f(rect);
  float stroke_half_width = paint.getStrokeWidth() / 2;
  rect_f.Inset(stroke_half_width, stroke_half_width);
  canvas->DrawRect(rect_f, paint);
}

}  // namespace

void Windows10CaptionButton::PaintSymbol(gfx::Canvas* canvas) {
  // TODO(bsep): This block is only correct for light themes.
  SkColor symbol_color;
  if (!frame_view_->ShouldPaintAsActive() && state() != STATE_HOVERED &&
      state() != STATE_PRESSED) {
    symbol_color = SkColorSetA(SK_ColorBLACK, 0x66);
  } else if (button_type_ == VIEW_ID_CLOSE_BUTTON &&
             hover_animation().is_animating()) {
    const int gray_value = gfx::Tween::IntValueBetween(
        hover_animation().GetCurrentValue(), 0x00, 0xFF);
    symbol_color = SkColorSetRGB(gray_value, gray_value, gray_value);
  } else if (button_type_ == VIEW_ID_CLOSE_BUTTON &&
             (state() == STATE_HOVERED || state() == STATE_PRESSED)) {
    symbol_color = SK_ColorWHITE;
  } else {
    symbol_color = SK_ColorBLACK;
  }

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  const int symbol_size_pixels = std::round(10 * scale);
  gfx::RectF bounds_rect(GetContentsBounds());
  bounds_rect.Scale(scale);
  gfx::Rect symbol_rect(gfx::ToEnclosingRect(bounds_rect));
  symbol_rect.ClampToCenteredSize(
      gfx::Size(symbol_size_pixels, symbol_size_pixels));

  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(symbol_color);
  paint.setStyle(SkPaint::kStroke_Style);
  // Stroke width jumps up a pixel every time we reach a new integral scale.
  const int stroke_width = std::floor(scale);
  paint.setStrokeWidth(stroke_width);

  switch (button_type_) {
    case VIEW_ID_MINIMIZE_BUTTON: {
      const int y = symbol_rect.CenterPoint().y();
      const gfx::Point p1 = gfx::Point(symbol_rect.x(), y);
      const gfx::Point p2 = gfx::Point(symbol_rect.right(), y);
      canvas->DrawLine(p1, p2, paint);
      return;
    }

    case VIEW_ID_MAXIMIZE_BUTTON:
      DrawRect(canvas, symbol_rect, paint);
      return;

    case VIEW_ID_RESTORE_BUTTON: {
      // Bottom left ("in front") square.
      const int separation = std::floor(2 * scale);
      symbol_rect.Inset(0, separation, separation, 0);
      DrawRect(canvas, symbol_rect, paint);

      // Top right ("behind") square.
      canvas->ClipRect(symbol_rect, SkRegion::kDifference_Op);
      symbol_rect.Offset(separation, -separation);
      DrawRect(canvas, symbol_rect, paint);
      return;
    }

    case VIEW_ID_CLOSE_BUTTON: {
      paint.setAntiAlias(true);
      // The close button's X is surrounded by a "halo" of transparent pixels.
      // When the X is white, the transparent pixels need to be a bit brighter
      // to be visible.
      const float stroke_halo =
          stroke_width * (symbol_color == SK_ColorWHITE ? 0.1f : 0.05f);
      paint.setStrokeWidth(stroke_width + stroke_halo);

      // TODO(bsep): This sometimes draws misaligned at fractional device scales
      // because the button's origin isn't necessarily aligned to pixels.
      canvas->ClipRect(symbol_rect);
      SkPath path;
      path.moveTo(symbol_rect.x(), symbol_rect.y());
      path.lineTo(symbol_rect.right(), symbol_rect.bottom());
      path.moveTo(symbol_rect.right(), symbol_rect.y());
      path.lineTo(symbol_rect.x(), symbol_rect.bottom());
      canvas->DrawPath(path, paint);
      return;
    }

    default:
      NOTREACHED();
      return;
  }
}
