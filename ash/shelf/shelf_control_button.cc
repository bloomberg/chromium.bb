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
  SetSize(gfx::Size(kShelfControlSize, kShelfControlSize));
}

ShelfControlButton::~ShelfControlButton() = default;

gfx::Point ShelfControlButton::GetCenterPoint() const {
  return gfx::Point(width() / 2.f, width() / 2.f);
}

std::unique_ptr<views::InkDropRipple> ShelfControlButton::CreateInkDropRipple()
    const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(),
      gfx::Insets(ShelfConstants::button_size() / 2 -
                  ShelfConstants::control_border_radius()),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropMask> ShelfControlButton::CreateInkDropMask()
    const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetCenterPoint(), ShelfConstants::control_border_radius());
}

gfx::Rect ShelfControlButton::CalculateButtonBounds() const {
  ShelfAlignment alignment = shelf_->alignment();
  gfx::Rect content_bounds = GetContentsBounds();
  // Align the button to the top of a bottom-aligned shelf, to the right edge
  // a left-aligned shelf, and to the left edge of a right-aligned shelf.
  const int inset = (ShelfConstants::shelf_size() - kShelfControlSize) / 2;
  const int x = alignment == SHELF_ALIGNMENT_LEFT
                    ? content_bounds.right() - inset - kShelfControlSize
                    : content_bounds.x() + inset;
  return gfx::Rect(x, content_bounds.y() + inset, kShelfControlSize,
                   kShelfControlSize);
}

void ShelfControlButton::PaintButtonContents(gfx::Canvas* canvas) {
  PaintBackground(canvas, CalculateButtonBounds());
}

void ShelfControlButton::PaintBackground(gfx::Canvas* canvas,
                                         const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(kShelfControlPermanentHighlightBackground);
  canvas->DrawRoundRect(bounds, ShelfConstants::control_border_radius(), flags);
}

}  // namespace ash
