// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/default_frame_header.h"

#include "ash/frame/caption_buttons/caption_button_model.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/frame_header_util.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/logging.h"  // DCHECK
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using views::Widget;

namespace {

// Color for the window title text.
const SkColor kTitleTextColor = SkColorSetRGB(40, 40, 40);
const SkColor kLightTitleTextColor = SK_ColorWHITE;

// Tiles an image into an area, rounding the top corners.
void TileRoundRect(gfx::Canvas* canvas,
                   const cc::PaintFlags& flags,
                   const gfx::Rect& bounds,
                   int corner_radius) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar corner_radius_scalar = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {corner_radius_scalar,
                       corner_radius_scalar,  // top-left
                       corner_radius_scalar,
                       corner_radius_scalar,  // top-right
                       0,
                       0,  // bottom-right
                       0,
                       0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  canvas->DrawPath(path, flags);
}

}  // namespace

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, public:

DefaultFrameHeader::DefaultFrameHeader(
    views::View* header_view,
    FrameCaptionButtonContainerView* caption_button_container)
    : active_frame_color_(kDefaultFrameColor),
      inactive_frame_color_(kDefaultFrameColor) {
  DCHECK(header_view);
  DCHECK(caption_button_container);
  set_view(header_view);
  SetCaptionButtonContainer(caption_button_container);
}

DefaultFrameHeader::~DefaultFrameHeader() = default;

void DefaultFrameHeader::SetThemeColor(SkColor theme_color) {
  set_button_color_mode(FrameCaptionButton::ColorMode::kThemed);
  SetFrameColorsImpl(theme_color, theme_color);
}

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, protected:

void DefaultFrameHeader::DoPaintHeader(gfx::Canvas* canvas) {
  int corner_radius =
      (GetWidget()->IsMaximized() || GetWidget()->IsFullscreen())
          ? 0
          : FrameHeaderUtil::GetTopCornerRadiusWhenRestored();

  cc::PaintFlags flags;
  int active_alpha = activation_animation().CurrentValueBetween(0, 255);
  flags.setColor(color_utils::AlphaBlend(active_frame_color_,
                                         inactive_frame_color_, active_alpha));
  flags.setAntiAlias(true);
  TileRoundRect(canvas, flags, GetPaintedBounds(), corner_radius);

  PaintTitleBar(canvas);
}

void DefaultFrameHeader::DoSetFrameColors(SkColor active_frame_color,
                                          SkColor inactive_frame_color) {
  set_button_color_mode(FrameCaptionButton::ColorMode::kDefault);
  SetFrameColorsImpl(active_frame_color, inactive_frame_color);
}

AshLayoutSize DefaultFrameHeader::GetButtonLayoutSize() const {
  return AshLayoutSize::kNonBrowserCaption;
}

SkColor DefaultFrameHeader::GetTitleColor() const {
  return color_utils::IsDark(GetCurrentFrameColor()) ? kLightTitleTextColor
                                                     : kTitleTextColor;
}

///////////////////////////////////////////////////////////////////////////////
// DefaultFrameHeader, private:

void DefaultFrameHeader::SetFrameColorsImpl(SkColor active_frame_color,
                                            SkColor inactive_frame_color) {
  bool updated = false;
  if (active_frame_color_ != active_frame_color) {
    active_frame_color_ = active_frame_color;
    updated = true;
  }
  if (inactive_frame_color_ != inactive_frame_color) {
    inactive_frame_color_ = inactive_frame_color;
    updated = true;
  }

  if (updated) {
    UpdateCaptionButtonColors();
    view()->SchedulePaint();
  }
}

SkColor DefaultFrameHeader::GetCurrentFrameColor() const {
  return mode() == MODE_ACTIVE ? active_frame_color_ : inactive_frame_color_;
}

}  // namespace ash
