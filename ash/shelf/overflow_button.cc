// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_button.h"

#include "ash/ash_switches.h"
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
namespace {

const int kButtonHoverAlpha = 150;

const int kButtonCornerRadius = 2;

const int kButtonHoverSize = 28;

const int kBackgroundOffset = (48 - kButtonHoverSize) / 2;

}  // namesapce

OverflowButton::OverflowButton(views::ButtonListener* listener)
    : CustomButton(listener),
      bottom_image_(NULL) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  bottom_image_ = rb.GetImageNamed(IDR_ASH_SHELF_OVERFLOW).ToImageSkia();


  SetAccessibilityFocusable(true);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_OVERFLOW_NAME));
}

OverflowButton::~OverflowButton() {}

void OverflowButton::OnShelfAlignmentChanged() {
  SchedulePaint();
}

void OverflowButton::PaintBackground(gfx::Canvas* canvas, int alpha) {
  gfx::Rect bounds(GetContentsBounds());
  gfx::Rect rect(0, 0, kButtonHoverSize, kButtonHoverSize);
  ShelfLayoutManager* shelf =
      ShelfLayoutManager::ForShelf(GetWidget()->GetNativeView());

  // Nudge the background a little to line up right.
  if (shelf->IsHorizontalAlignment()) {
    rect.set_origin(gfx::Point(
        bounds.x() + ((bounds.width() - kButtonHoverSize) / 2) - 1,
        bounds.y() + kBackgroundOffset - 1));

  } else {
    rect.set_origin(gfx::Point(
        bounds.x() + kBackgroundOffset - 1,
        bounds.y() + ((bounds.height() - kButtonHoverSize) / 2) - 1));
  }

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SkColorSetARGB(
      kButtonHoverAlpha * hover_animation_->GetCurrentValue(),
      0, 0, 0));

  const SkScalar radius = SkIntToScalar(kButtonCornerRadius);
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(rect), radius, radius);
  canvas->DrawPath(path, paint);
}

void OverflowButton::OnPaint(gfx::Canvas* canvas) {
  ShelfLayoutManager* layout_manager =
      ShelfLayoutManager::ForShelf(GetWidget()->GetNativeView());
  ShelfAlignment alignment = layout_manager->GetAlignment();

  gfx::Rect bounds(GetContentsBounds());
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int background_image_id = 0;
  if (layout_manager->shelf_widget()->shelf()->IsShowingOverflowBubble())
    background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_PRESSED;
  else if(layout_manager->shelf_widget()->GetDimsShelf())
    background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_ON_BLACK;
  else
    background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_NORMAL;

  const gfx::ImageSkia* background =
      rb.GetImageNamed(background_image_id).ToImageSkia();
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
  canvas->DrawImageInt(*background, bounds.x(), bounds.y());

  if (height() < kButtonHoverSize)
    return;

  const gfx::ImageSkia* image = NULL;

  switch(alignment) {
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

}  // namespace ash
