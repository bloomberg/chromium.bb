// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/overflow_button.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/ink_drop_button_listener.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector_icons_public.h"

namespace ash {

OverflowButton::OverflowButton(ShelfView* shelf_view, WmShelf* wm_shelf)
    : CustomButton(nullptr),
      bottom_image_(nullptr),
      shelf_view_(shelf_view),
      wm_shelf_(wm_shelf),
      background_alpha_(0) {
  DCHECK(shelf_view_);
  if (MaterialDesignController::IsShelfMaterial()) {
    bottom_image_md_ =
        CreateVectorIcon(gfx::VectorIconId::SHELF_OVERFLOW, kShelfIconColor);
    bottom_image_ = &bottom_image_md_;
  } else {
    bottom_image_ = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_ASH_SHELF_OVERFLOW);
  }

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_OVERFLOW_NAME));
}

OverflowButton::~OverflowButton() {}

void OverflowButton::OnShelfAlignmentChanged() {
  SchedulePaint();
}

void OverflowButton::SetBackgroundAlpha(int alpha) {
  background_alpha_ = alpha;
  SchedulePaint();
}

void OverflowButton::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = CalculateButtonBounds();
  PaintBackground(canvas, bounds);
  PaintForeground(canvas, bounds);
}

void OverflowButton::NotifyClick(const ui::Event& event) {
  CustomButton::NotifyClick(event);
  shelf_view_->ButtonPressed(this, event, ink_drop());
}

void OverflowButton::PaintBackground(gfx::Canvas* canvas,
                                     const gfx::Rect& bounds) {
  if (MaterialDesignController::IsShelfMaterial()) {
    SkPaint background_paint;
    background_paint.setFlags(SkPaint::kAntiAlias_Flag);
    background_paint.setColor(SkColorSetA(kShelfBaseColor, background_alpha_));
    canvas->DrawRoundRect(bounds, kOverflowButtonCornerRadius,
                          background_paint);

    if (shelf_view_->IsShowingOverflowBubble()) {
      SkPaint highlight_paint;
      highlight_paint.setFlags(SkPaint::kAntiAlias_Flag);
      highlight_paint.setColor(kShelfButtonActivatedHighlightColor);
      canvas->DrawRoundRect(bounds, kOverflowButtonCornerRadius,
                            highlight_paint);
    }
  } else {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* background =
        rb.GetImageNamed(NonMaterialBackgroundImageId()).ToImageSkia();
    canvas->DrawImageInt(*background, bounds.x(), bounds.y());
  }
}

void OverflowButton::PaintForeground(gfx::Canvas* canvas,
                                     const gfx::Rect& bounds) {
  const gfx::ImageSkia* image = nullptr;

  switch (wm_shelf_->GetAlignment()) {
    case SHELF_ALIGNMENT_LEFT:
      if (left_image_.isNull()) {
        left_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
            *bottom_image_, SkBitmapOperations::ROTATION_90_CW);
      }
      image = &left_image_;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      if (right_image_.isNull()) {
        right_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
            *bottom_image_, SkBitmapOperations::ROTATION_270_CW);
      }
      image = &right_image_;
      break;
    default:
      image = bottom_image_;
      break;
  }

  canvas->DrawImageInt(*image,
                       bounds.x() + ((bounds.width() - image->width()) / 2),
                       bounds.y() + ((bounds.height() - image->height()) / 2));
}

int OverflowButton::NonMaterialBackgroundImageId() {
  if (shelf_view_->IsShowingOverflowBubble())
    return IDR_AURA_NOTIFICATION_BACKGROUND_PRESSED;
  else if (wm_shelf_->IsDimmed())
    return IDR_AURA_NOTIFICATION_BACKGROUND_ON_BLACK;
  return IDR_AURA_NOTIFICATION_BACKGROUND_NORMAL;
}

gfx::Rect OverflowButton::CalculateButtonBounds() {
  ShelfAlignment alignment = wm_shelf_->GetAlignment();
  gfx::Rect bounds(GetContentsBounds());
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (MaterialDesignController::IsShelfMaterial()) {
    const int width_offset = (bounds.width() - kOverflowButtonSize) / 2;
    const int height_offset = (bounds.height() - kOverflowButtonSize) / 2;
    if (IsHorizontalAlignment(alignment)) {
      bounds = gfx::Rect(bounds.x() + width_offset, bounds.y() + height_offset,
                         kOverflowButtonSize, kOverflowButtonSize);
    } else {
      bounds = gfx::Rect(bounds.x() + height_offset, bounds.y() + width_offset,
                         kOverflowButtonSize, kOverflowButtonSize);
    }
  } else {
    const gfx::ImageSkia* background =
        rb.GetImageNamed(NonMaterialBackgroundImageId()).ToImageSkia();
    if (alignment == SHELF_ALIGNMENT_LEFT) {
      bounds =
          gfx::Rect(bounds.right() - background->width() - kShelfItemInset,
                    bounds.y() + (bounds.height() - background->height()) / 2,
                    background->width(), background->height());
    } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
      bounds =
          gfx::Rect(bounds.x() + kShelfItemInset,
                    bounds.y() + (bounds.height() - background->height()) / 2,
                    background->width(), background->height());
    } else {
      bounds =
          gfx::Rect(bounds.x() + (bounds.width() - background->width()) / 2,
                    bounds.y() + kShelfItemInset, background->width(),
                    background->height());
    }
  }
  return bounds;
}

}  // namespace ash
