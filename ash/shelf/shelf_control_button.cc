// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_control_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/widget/widget.h"

namespace ash {

ShelfControlButton::ShelfControlButton(ShelfView* shelf_view)
    : ShelfButton(shelf_view), shelf_(shelf_view->shelf()) {
  set_has_ink_drop_action_on_click(true);
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return gfx::Point(width() / 2.f, height() / 2.f);
}

std::unique_ptr<views::InkDropRipple> ShelfControlButton::CreateInkDropRipple()
    const {
  const int button_radius = ShelfConstants::control_border_radius();
  gfx::Point center = GetCenterPoint();
  gfx::Rect bounds(center.x() - button_radius, center.y() - button_radius,
                   2 * button_radius, 2 * button_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropMask> ShelfControlButton::CreateInkDropMask()
    const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetCenterPoint(), ShelfConstants::control_border_radius());
}

const char* ShelfControlButton::GetClassName() const {
  return "ash/ShelfControlButton";
}

void ShelfControlButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintBackground(canvas, GetContentsBounds());
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kShelfControlPermanentHighlightBackground);
  canvas->DrawRoundRect(bounds, ShelfConstants::control_border_radius(), flags);
}

}  // namespace ash
