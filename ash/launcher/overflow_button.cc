// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/overflow_button.h"

#include "ash/wm/shelf_layout_manager.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

const int kButtonHoverAlpha = 150;

const int kButtonCornerRadius = 2;

const int kButtonHoverSize = 28;

const int kBackgroundOffset = (48 - kButtonHoverSize) / 2;

void RotateCounterclockwise(gfx::Transform* transform) {
  SkMatrix44 rotation;
  rotation.set3x3(0, -1, 0,
                  1,  0, 0,
                  0,  0, 1);
  transform->matrix().preConcat(rotation);
}

void RotateClockwise(gfx::Transform* transform) {
  SkMatrix44 rotation;
  rotation.set3x3( 0, 1, 0,
                  -1, 0, 0,
                   0, 0, 1);
  transform->matrix().preConcat(rotation);
}

}  // namesapce

OverflowButton::OverflowButton(views::ButtonListener* listener)
    : CustomButton(listener),
      image_(NULL) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  image_ = rb.GetImageNamed(IDR_AURA_LAUNCHER_OVERFLOW).ToImageSkia();

  set_accessibility_focusable(true);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AURA_LAUNCHER_OVERFLOW_NAME));
}


OverflowButton::~OverflowButton() {
}

void OverflowButton::OnShelfAlignmentChanged() {
  SchedulePaint();
}

void OverflowButton::PaintBackground(gfx::Canvas* canvas, int alpha) {
  gfx::Rect bounds(GetContentsBounds());
  gfx::Rect rect(0, 0, kButtonHoverSize, kButtonHoverSize);
  ShelfLayoutManager* shelf =
      ShelfLayoutManager::ForLauncher(GetWidget()->GetNativeView());

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
  ShelfAlignment alignment = ShelfLayoutManager::ForLauncher(
      GetWidget()->GetNativeView())->GetAlignment();

  if (hover_animation_->is_animating()) {
    PaintBackground(
        canvas,
        kButtonHoverAlpha * hover_animation_->GetCurrentValue());
  } else if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    PaintBackground(canvas, kButtonHoverAlpha);
  }

  if (height() < kButtonHoverSize)
    return;

  gfx::Transform transform;

  switch (alignment) {
    case SHELF_ALIGNMENT_BOTTOM:
      // Shift 1 pixel left to align with overflow bubble tip.
      transform.Translate(-1, kBackgroundOffset);
      break;
    case SHELF_ALIGNMENT_LEFT:
      transform.Translate(kBackgroundOffset, -1);
      RotateClockwise(&transform);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      transform.Translate(kBackgroundOffset, height());
      RotateCounterclockwise(&transform);
      break;
    case SHELF_ALIGNMENT_TOP:
      transform.Translate(1, kBackgroundOffset);
      break;
  }

  canvas->Save();
  canvas->Transform(transform);

  gfx::Rect rect(GetContentsBounds());
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    canvas->DrawImageInt(*image_,
                         rect.x() + (rect.width() - image_->width()) / 2,
                         kButtonHoverSize - image_->height());
  } else {
    canvas->DrawImageInt(*image_,
                         kButtonHoverSize - image_->width(),
                         rect.y() + (rect.height() - image_->height()) / 2);
  }
  canvas->Restore();
}

}  // namespace internal
}  // namespace ash
