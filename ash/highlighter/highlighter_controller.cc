// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include "ash/highlighter/highlighter_gesture_util.h"
#include "ash/highlighter/highlighter_result_view.h"
#include "ash/highlighter/highlighter_selection_observer.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/public/cpp/scale_utility.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Adjust the height of the bounding box to match the pen tip height,
// while keeping the same vertical center line. Adjust the width to
// account for the pen tip width.
gfx::RectF AdjustHorizontalStroke(const gfx::RectF& box,
                                  const gfx::SizeF& pen_tip_size) {
  return gfx::RectF(box.x() - pen_tip_size.width() / 2,
                    box.CenterPoint().y() - pen_tip_size.height() / 2,
                    box.width() + pen_tip_size.width(), pen_tip_size.height());
}

// This method computes the scale required to convert window-relative DIP
// coordinates to the coordinate space of the screenshot taken from that window.
// The transform returned by WindowTreeHost::GetRootTransform translates points
// from DIP to physical screen pixels (by taking into account not only the
// scale but also the rotation and the offset).
// However, the screenshot bitmap is always oriented the same way as the window
// from which it was taken, and has zero offset.
// The code below deduces the scale from the transform by applying it to a pair
// of points separated by the distance of 1, and measuring the distance between
// the transformed points.
float GetScreenshotScale(aura::Window* window) {
  return GetScaleFactorForTransform(window->GetHost()->GetRootTransform());
}

}  // namespace

HighlighterController::HighlighterController() {}

HighlighterController::~HighlighterController() {}

void HighlighterController::SetObserver(
    HighlighterSelectionObserver* observer) {
  observer_ = observer;
}

views::View* HighlighterController::GetPointerView() const {
  return highlighter_view_.get();
}

void HighlighterController::CreatePointerView(
    base::TimeDelta presentation_delay,
    aura::Window* root_window) {
  highlighter_view_ =
      base::MakeUnique<HighlighterView>(presentation_delay, root_window);
  result_view_.reset();
}

void HighlighterController::UpdatePointerView(ui::TouchEvent* event) {
  highlighter_view_->AddNewPoint(event->root_location_f(), event->time_stamp());

  if (event->type() == ui::ET_TOUCH_RELEASED) {
    aura::Window* current_window =
        highlighter_view_->GetWidget()->GetNativeWindow()->GetRootWindow();

    const FastInkPoints& points = highlighter_view_->points();
    gfx::RectF box = points.GetBoundingBoxF();

    HighlighterView::AnimationMode animation_mode;
    if (DetectHorizontalStroke(box, HighlighterView::kPenTipSize)) {
      box = AdjustHorizontalStroke(box, HighlighterView::kPenTipSize);
      animation_mode = HighlighterView::AnimationMode::kFadeout;
    } else if (DetectClosedShape(box, points)) {
      animation_mode = HighlighterView::AnimationMode::kInflate;
    } else {
      animation_mode = HighlighterView::AnimationMode::kDeflate;
    }

    highlighter_view_->Animate(
        box.CenterPoint(), animation_mode,
        base::Bind(&HighlighterController::DestroyHighlighterView,
                   base::Unretained(this)));

    if (animation_mode != HighlighterView::AnimationMode::kDeflate) {
      if (observer_) {
        observer_->HandleSelection(gfx::ToEnclosingRect(
            gfx::ScaleRect(box, GetScreenshotScale(current_window))));
      }

      result_view_ = base::MakeUnique<HighlighterResultView>(current_window);
      result_view_->Animate(
          box, animation_mode,
          base::Bind(&HighlighterController::DestroyResultView,
                     base::Unretained(this)));
    }
  }
}

void HighlighterController::DestroyPointerView() {
  DestroyHighlighterView();
  DestroyResultView();
}

void HighlighterController::DestroyHighlighterView() {
  highlighter_view_.reset();
}

void HighlighterController::DestroyResultView() {
  result_view_.reset();
}

}  // namespace ash
