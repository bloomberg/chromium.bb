// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/windows_10_caption_button.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_utils.h"
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

namespace {
SkAlpha ButtonBackgroundAlpha(SkAlpha theme_alpha) {
  return theme_alpha == SK_AlphaOPAQUE ? 0xCC : theme_alpha;
}
}

SkColor Windows10CaptionButton::GetBaseColor() const {
  const SkColor titlebar_color = frame_view_->GetTitlebarColor();
  const SkColor bg_color =
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_BUTTON_BACKGROUND);
  const SkAlpha theme_alpha = SkColorGetA(bg_color);
  const SkColor blend_color =
      theme_alpha > 0
          ? color_utils::AlphaBlend(bg_color, titlebar_color,
                                    ButtonBackgroundAlpha(theme_alpha))
          : titlebar_color;
  return color_utils::IsDark(blend_color) ? SK_ColorWHITE : SK_ColorBLACK;
}

void Windows10CaptionButton::OnPaint(gfx::Canvas* canvas) {
  PaintBackground(canvas);
  PaintSymbol(canvas);
}

void Windows10CaptionButton::PaintBackground(gfx::Canvas* canvas) {
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  const SkColor bg_color =
      theme_provider->GetColor(ThemeProperties::COLOR_BUTTON_BACKGROUND);
  const SkAlpha theme_alpha = SkColorGetA(bg_color);
  if (theme_alpha > 0) {
    canvas->FillRect(GetContentsBounds(),
                     SkColorSetA(bg_color, ButtonBackgroundAlpha(theme_alpha)));
  }
  if (theme_provider->HasCustomImage(IDR_THEME_WINDOW_CONTROL_BACKGROUND)) {
    const gfx::Rect bounds = GetContentsBounds();
    canvas->TileImageInt(
        *theme_provider->GetImageSkiaNamed(IDR_THEME_WINDOW_CONTROL_BACKGROUND),
        0, 0, bounds.width(), bounds.height());
  }

  SkColor base_color;
  SkAlpha hovered_alpha, pressed_alpha;
  if (button_type_ == VIEW_ID_CLOSE_BUTTON) {
    base_color = SkColorSetRGB(0xE8, 0x11, 0x23);
    hovered_alpha = SK_AlphaOPAQUE;
    pressed_alpha = 0x98;
  } else {
    // Match the native buttons.
    base_color = GetBaseColor();
    hovered_alpha = 0x1A;
    pressed_alpha = 0x33;

    if (theme_alpha > 0) {
      // Theme buttons have slightly increased opacity to make them stand out
      // against a visually-busy frame image.
      constexpr float kAlphaScale = 1.3f;
      hovered_alpha = gfx::ToRoundedInt(hovered_alpha * kAlphaScale);
      pressed_alpha = gfx::ToRoundedInt(pressed_alpha * kAlphaScale);
    }
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
              const cc::PaintFlags& flags) {
  gfx::RectF rect_f(rect);
  float stroke_half_width = flags.getStrokeWidth() / 2;
  rect_f.Inset(stroke_half_width, stroke_half_width);
  canvas->DrawRect(rect_f, flags);
}

}  // namespace

void Windows10CaptionButton::PaintSymbol(gfx::Canvas* canvas) {
  SkColor symbol_color = GetBaseColor();
  if (!frame_view_->ShouldPaintAsActive() && state() != STATE_HOVERED &&
      state() != STATE_PRESSED) {
    symbol_color = SkColorSetA(symbol_color, 0x65);
  } else if (button_type_ == VIEW_ID_CLOSE_BUTTON &&
             hover_animation().is_animating()) {
    symbol_color = gfx::Tween::ColorValueBetween(
        hover_animation().GetCurrentValue(), symbol_color, SK_ColorWHITE);
  } else if (button_type_ == VIEW_ID_CLOSE_BUTTON &&
             (state() == STATE_HOVERED || state() == STATE_PRESSED)) {
    symbol_color = SK_ColorWHITE;
  }

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  const int symbol_size_pixels = std::round(10 * scale);
  gfx::RectF bounds_rect(GetContentsBounds());
  bounds_rect.Scale(scale);
  gfx::Rect symbol_rect(gfx::ToEnclosingRect(bounds_rect));
  symbol_rect.ClampToCenteredSize(
      gfx::Size(symbol_size_pixels, symbol_size_pixels));

  cc::PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setColor(symbol_color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // Stroke width jumps up a pixel every time we reach a new integral scale.
  const int stroke_width = std::floor(scale);
  flags.setStrokeWidth(stroke_width);

  switch (button_type_) {
    case VIEW_ID_MINIMIZE_BUTTON: {
      const int y = symbol_rect.CenterPoint().y();
      const gfx::Point p1 = gfx::Point(symbol_rect.x(), y);
      const gfx::Point p2 = gfx::Point(symbol_rect.right(), y);
      canvas->DrawLine(p1, p2, flags);
      return;
    }

    case VIEW_ID_MAXIMIZE_BUTTON:
      DrawRect(canvas, symbol_rect, flags);
      return;

    case VIEW_ID_RESTORE_BUTTON: {
      // Bottom left ("in front") square.
      const int separation = std::floor(2 * scale);
      symbol_rect.Inset(0, separation, separation, 0);
      DrawRect(canvas, symbol_rect, flags);

      // Top right ("behind") square.
      canvas->ClipRect(symbol_rect, SkClipOp::kDifference);
      symbol_rect.Offset(separation, -separation);
      DrawRect(canvas, symbol_rect, flags);
      return;
    }

    case VIEW_ID_CLOSE_BUTTON: {
      flags.setAntiAlias(true);
      // The close button's X is surrounded by a "halo" of transparent pixels.
      // When the X is white, the transparent pixels need to be a bit brighter
      // to be visible.
      const float stroke_halo =
          stroke_width * (symbol_color == SK_ColorWHITE ? 0.1f : 0.05f);
      flags.setStrokeWidth(stroke_width + stroke_halo);

      // TODO(bsep): This sometimes draws misaligned at fractional device scales
      // because the button's origin isn't necessarily aligned to pixels.
      canvas->ClipRect(symbol_rect);
      SkPath path;
      path.moveTo(symbol_rect.x(), symbol_rect.y());
      path.lineTo(symbol_rect.right(), symbol_rect.bottom());
      path.moveTo(symbol_rect.right(), symbol_rect.y());
      path.lineTo(symbol_rect.x(), symbol_rect.bottom());
      canvas->DrawPath(path, flags);
      return;
    }

    default:
      NOTREACHED();
      return;
  }
}
