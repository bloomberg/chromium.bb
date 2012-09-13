// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/overflow_button.h"

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

namespace ash {
namespace internal {

namespace {

const int kButtonHoverAlpha = 150;

const int kButtonCornerRadius = 2;

const int kButtonHoverSize = 28;

void RotateCounterclockwise(ui::Transform* transform) {
  transform->matrix().set3x3(0, -1, 0,
                             1,  0, 0,
                             0,  0, 1);
}

void RotateClockwise(ui::Transform* transform) {
  transform->matrix().set3x3( 0, 1, 0,
                             -1, 0, 0,
                              0, 0, 1);
}

}  // namesapce

OverflowButton::OverflowButton(views::ButtonListener* listener)
    : CustomButton(listener),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      image_(NULL) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  image_ = rb.GetImageNamed(IDR_AURA_LAUNCHER_OVERFLOW).ToImageSkia();

  set_accessibility_focusable(true);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AURA_LAUNCHER_OVERFLOW_NAME));
}


OverflowButton::~OverflowButton() {
}

void OverflowButton::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  SchedulePaint();
}

void OverflowButton::PaintBackground(gfx::Canvas* canvas, int alpha) {
  gfx::Rect rect(GetContentsBounds());
  rect = rect.Center(gfx::Size(kButtonHoverSize, kButtonHoverSize));

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
  if (hover_animation_->is_animating()) {
    PaintBackground(
        canvas,
        kButtonHoverAlpha * hover_animation_->GetCurrentValue());
  } else if (state() == BS_HOT || state() == BS_PUSHED) {
    PaintBackground(canvas, kButtonHoverAlpha);
  }

  ui::Transform transform;

  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
      // Shift 1 pixel left to align with overflow bubble tip.
      transform.ConcatTranslate(-1, 0);
      break;
    case SHELF_ALIGNMENT_LEFT:
      RotateClockwise(&transform);
      transform.ConcatTranslate(width(), -1);
      break;
    case SHELF_ALIGNMENT_RIGHT:
      RotateCounterclockwise(&transform);
      transform.ConcatTranslate(0, height());
      break;
  }

  canvas->Save();
  canvas->Transform(transform);

  gfx::Rect rect(GetContentsBounds());
  canvas->DrawImageInt(*image_,
                       rect.x() + (rect.width() - image_->width()) / 2,
                       rect.y() + (rect.height() - image_->height()) / 2);

  canvas->Restore();
}

}  // namespace internal
}  // namespace ash
