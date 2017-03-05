// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/overflow_button.h"

#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"

namespace ash {

OverflowButton::OverflowButton(ShelfView* shelf_view, WmShelf* wm_shelf)
    : CustomButton(nullptr),
      shelf_view_(shelf_view),
      wm_shelf_(wm_shelf),
      background_color_(kShelfDefaultBaseColor) {
  DCHECK(shelf_view_);

  SetInkDropMode(InkDropMode::ON);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  set_hide_ink_drop_when_showing_context_menu(false);
  bottom_image_ = gfx::CreateVectorIcon(kShelfOverflowIcon, kShelfIconColor);

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_OVERFLOW_NAME));
}

OverflowButton::~OverflowButton() {}

void OverflowButton::OnShelfAlignmentChanged() {
  SchedulePaint();
}

void OverflowButton::OnOverflowBubbleShown() {
  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
}

void OverflowButton::OnOverflowBubbleHidden() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
}

void OverflowButton::UpdateShelfItemBackground(SkColor color) {
  background_color_ = color;
  SchedulePaint();
}

void OverflowButton::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = CalculateButtonBounds();
  PaintBackground(canvas, bounds);
  PaintForeground(canvas, bounds);
}

std::unique_ptr<views::InkDrop> OverflowButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CreateDefaultFloodFillInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> OverflowButton::CreateInkDropRipple()
    const {
  gfx::Insets insets = GetLocalBounds().InsetsFrom(CalculateButtonBounds());
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), insets, GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

bool OverflowButton::ShouldEnterPushedState(const ui::Event& event) {
  if (shelf_view_->IsShowingOverflowBubble())
    return false;

  return CustomButton::ShouldEnterPushedState(event);
}

void OverflowButton::NotifyClick(const ui::Event& event) {
  CustomButton::NotifyClick(event);
  shelf_view_->ButtonPressed(this, event, GetInkDrop());
}

std::unique_ptr<views::InkDropMask> OverflowButton::CreateInkDropMask() const {
  gfx::Insets insets = GetLocalBounds().InsetsFrom(CalculateButtonBounds());
  return base::MakeUnique<views::RoundRectInkDropMask>(
      size(), insets, kOverflowButtonCornerRadius);
}

void OverflowButton::PaintBackground(gfx::Canvas* canvas,
                                     const gfx::Rect& bounds) {
  cc::PaintFlags flags;
  flags.setFlags(cc::PaintFlags::kAntiAlias_Flag);
  flags.setColor(background_color_);
  canvas->DrawRoundRect(bounds, kOverflowButtonCornerRadius, flags);
}

void OverflowButton::PaintForeground(gfx::Canvas* canvas,
                                     const gfx::Rect& bounds) {
  const gfx::ImageSkia* image = nullptr;

  switch (wm_shelf_->GetAlignment()) {
    case SHELF_ALIGNMENT_LEFT:
      if (left_image_.isNull()) {
        left_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
            bottom_image_, SkBitmapOperations::ROTATION_90_CW);
      }
      image = &left_image_;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (right_image_.isNull()) {
        right_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
            bottom_image_, SkBitmapOperations::ROTATION_270_CW);
      }
      image = &right_image_;
      break;
    default:
      image = &bottom_image_;
      break;
  }

  canvas->DrawImageInt(*image,
                       bounds.x() + ((bounds.width() - image->width()) / 2),
                       bounds.y() + ((bounds.height() - image->height()) / 2));
}

gfx::Rect OverflowButton::CalculateButtonBounds() const {
  ShelfAlignment alignment = wm_shelf_->GetAlignment();
  gfx::Rect content_bounds = GetContentsBounds();
  // Align the button to the top of a bottom-aligned shelf, to the right edge
  // a left-aligned shelf, and to the left edge of a right-aligned shelf.
  const int inset = (GetShelfConstant(SHELF_SIZE) - kOverflowButtonSize) / 2;
  const int x = alignment == SHELF_ALIGNMENT_LEFT
                    ? content_bounds.right() - inset - kOverflowButtonSize
                    : content_bounds.x() + inset;
  return gfx::Rect(x, content_bounds.y() + inset, kOverflowButtonSize,
                   kOverflowButtonSize);
}

}  // namespace ash
