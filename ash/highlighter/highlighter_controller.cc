// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include "ash/highlighter/highlighter_gesture_util.h"
#include "ash/highlighter/highlighter_result_view.h"
#include "ash/highlighter/highlighter_selection_observer.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/public/cpp/scale_utility.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
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

void HighlighterController::SetEnabled(bool enabled) {
  FastInkPointerController::SetEnabled(enabled);
  if (enabled) {
    session_start_ = ui::EventTimeForNow();
    gesture_counter_ = 0;
    recognized_gesture_counter_ = 0;
  } else {
    UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.Palette.Assistant.GesturesPerSession",
                             gesture_counter_);
    UMA_HISTOGRAM_COUNTS_100(
        "Ash.Shelf.Palette.Assistant.GesturesPerSession.Recognized",
        recognized_gesture_counter_);
  }
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

    const HighlighterGestureType gesture_type =
        DetectHighlighterGesture(box, HighlighterView::kPenTipSize, points);

    if (gesture_type == HighlighterGestureType::kHorizontalStroke) {
      UMA_HISTOGRAM_COUNTS_10000(
          "Ash.Shelf.Palette.Assistant.HighlighterLength",
          static_cast<int>(box.width()));

      box = AdjustHorizontalStroke(box, HighlighterView::kPenTipSize);
    } else if (gesture_type == HighlighterGestureType::kClosedShape) {
      const float fraction = box.width() * box.height() /
                             (current_window->bounds().width() *
                              current_window->bounds().height());
      UMA_HISTOGRAM_PERCENTAGE("Ash.Shelf.Palette.Assistant.CircledPercentage",
                               static_cast<int>(fraction * 100));
    }

    highlighter_view_->Animate(
        box.CenterPoint(), gesture_type,
        base::Bind(&HighlighterController::DestroyHighlighterView,
                   base::Unretained(this)));

    if (gesture_type != HighlighterGestureType::kNotRecognized) {
      if (observer_) {
        observer_->HandleSelection(gfx::ToEnclosingRect(
            gfx::ScaleRect(box, GetScreenshotScale(current_window))));
      }

      result_view_ = base::MakeUnique<HighlighterResultView>(current_window);
      result_view_->Animate(
          box, gesture_type,
          base::Bind(&HighlighterController::DestroyResultView,
                     base::Unretained(this)));

      recognized_gesture_counter_++;
    }

    gesture_counter_++;

    const base::TimeTicks gesture_start = points.GetOldest().time;
    if (gesture_counter_ > 1) {
      // Up to 3 minutes.
      UMA_HISTOGRAM_MEDIUM_TIMES("Ash.Shelf.Palette.Assistant.GestureInterval",
                                 gesture_start - previous_gesture_end_);
    }
    previous_gesture_end_ = points.GetNewest().time;

    // Up to 10 seconds.
    UMA_HISTOGRAM_TIMES("Ash.Shelf.Palette.Assistant.GestureDuration",
                        points.GetNewest().time - gesture_start);

    UMA_HISTOGRAM_ENUMERATION("Ash.Shelf.Palette.Assistant.GestureType",
                              gesture_type,
                              HighlighterGestureType::kGestureCount);
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
