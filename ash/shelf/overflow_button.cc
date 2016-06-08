// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_button.h"

#include "ash/ash_switches.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverflowButton::OverflowButton(views::ButtonListener* listener, Shelf* shelf)
    : CustomButton(listener), bottom_image_(nullptr), shelf_(shelf) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  bottom_image_ = rb->GetImageNamed(IDR_ASH_SHELF_OVERFLOW).ToImageSkia();

  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_OVERFLOW_NAME));
}

OverflowButton::~OverflowButton() {}

void OverflowButton::OnShelfAlignmentChanged() {
  SchedulePaint();
}

void OverflowButton::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect bounds = CalculateButtonBounds();
  PaintBackground(canvas, bounds);
  PaintForeground(canvas, bounds);
}

void OverflowButton::PaintBackground(gfx::Canvas* canvas,
                                     const gfx::Rect& bounds) {
  if (MaterialDesignController::IsShelfMaterial()) {
    SkColor color = SK_ColorTRANSPARENT;
    ShelfWidget* shelf_widget = shelf_->shelf_widget();
    if (shelf_->IsShowingOverflowBubble()) {
      // TODO(bruthig|mohsen): Use of this color is temporary. Draw the active
      // state using the material design ripple animation.
      color = SK_ColorBLUE;
    } else if (shelf_widget &&
               shelf_widget->GetBackgroundType() ==
                   ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT) {
      color = SkColorSetA(kShelfBaseColor,
                          GetShelfConstant(SHELF_BACKGROUND_ALPHA));
    }

    // TODO(bruthig|tdanderson): The background should be changed using a
    // fade in/out animation.
    const int kCornerRadius = 2;
    SkPaint paint;
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(color);
    canvas->DrawRoundRect(bounds, kCornerRadius, paint);
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

  switch(shelf_->alignment()) {
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
  if (shelf_->IsShowingOverflowBubble())
    return IDR_AURA_NOTIFICATION_BACKGROUND_PRESSED;
  else if (shelf_->shelf_widget()->GetDimsShelf())
    return IDR_AURA_NOTIFICATION_BACKGROUND_ON_BLACK;
  return IDR_AURA_NOTIFICATION_BACKGROUND_NORMAL;
}

gfx::Rect OverflowButton::CalculateButtonBounds() {
  ShelfAlignment alignment = shelf_->alignment();
  gfx::Rect bounds(GetContentsBounds());
  if (!MaterialDesignController::IsShelfMaterial()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* background =
        rb.GetImageNamed(NonMaterialBackgroundImageId()).ToImageSkia();
    if (alignment == SHELF_ALIGNMENT_LEFT) {
      bounds = gfx::Rect(
          bounds.right() - background->width() -
              ShelfLayoutManager::kShelfItemInset,
          bounds.y() + (bounds.height() - background->height()) / 2,
          background->width(), background->height());
    } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
      bounds = gfx::Rect(
          bounds.x() + ShelfLayoutManager::kShelfItemInset,
          bounds.y() + (bounds.height() - background->height()) / 2,
          background->width(), background->height());
    } else {
      bounds = gfx::Rect(
          bounds.x() + (bounds.width() - background->width()) / 2,
          bounds.y() + ShelfLayoutManager::kShelfItemInset,
          background->width(), background->height());
    }
  }

  return bounds;
}

}  // namespace ash
