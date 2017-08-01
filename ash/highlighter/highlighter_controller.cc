// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_controller.h"

#include "ash/ash_switches.h"
#include "ash/highlighter/highlighter_gesture_util.h"
#include "ash/highlighter/highlighter_result_view.h"
#include "ash/highlighter/highlighter_selection_observer.h"
#include "ash/highlighter/highlighter_view.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_utils.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Adjust the height of the bounding box to match the pen tip height,
// while keeping the same vertical center line. Adjust the width to
// account for the pen tip width.
gfx::Rect AdjustHorizontalStroke(const gfx::Rect& box,
                                 const gfx::SizeF& pen_tip_size) {
  return gfx::ToEnclosingRect(
      gfx::RectF(box.x() - pen_tip_size.width() / 2,
                 box.CenterPoint().y() - pen_tip_size.height() / 2,
                 box.width() + pen_tip_size.width(), pen_tip_size.height()));
}

// The default amount of time used to estimate time from VSYNC event to when
// visible light can be noticed by the user. This is used when a device
// specific estimate was not provided using --estimated-presentation-delay.
const int kDefaultPresentationDelayMs = 18;

}  // namespace

HighlighterController::HighlighterController() : observer_(nullptr) {
  Shell::Get()->AddPreTargetHandler(this);

  // The code below is copied from laser_pointer_controller.cc
  // TODO(kaznacheev) extract common base class (crbug.com/750235)
  int64_t presentation_delay_ms;
  // Use device specific presentation delay if specified.
  std::string presentation_delay_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshEstimatedPresentationDelay);
  if (!base::StringToInt64(presentation_delay_string, &presentation_delay_ms))
    presentation_delay_ms = kDefaultPresentationDelayMs;
  presentation_delay_ =
      base::TimeDelta::FromMilliseconds(presentation_delay_ms);
}

HighlighterController::~HighlighterController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void HighlighterController::EnableHighlighter(
    HighlighterSelectionObserver* observer) {
  observer_ = observer;
}

void HighlighterController::DisableHighlighter() {
  observer_ = nullptr;
  DestroyHighlighterView();
}

void HighlighterController::OnTouchEvent(ui::TouchEvent* event) {
  if (!observer_)
    return;

  if (event->pointer_details().pointer_type !=
      ui::EventPointerType::POINTER_TYPE_PEN)
    return;

  if (event->type() != ui::ET_TOUCH_MOVED &&
      event->type() != ui::ET_TOUCH_PRESSED &&
      event->type() != ui::ET_TOUCH_RELEASED)
    return;

  // Find the root window that the event was captured on. We never need to
  // switch between different root windows because it is not physically possible
  // to seamlessly drag a finger between two displays like it is with a mouse.
  gfx::Point event_location = event->root_location();
  aura::Window* current_window =
      static_cast<aura::Window*>(event->target())->GetRootWindow();

  // Start a new highlighter session if the stylus is pressed but not
  // pressed over the palette.
  if (event->type() == ui::ET_TOUCH_PRESSED &&
      !palette_utils::PaletteContainsPointInScreen(event_location)) {
    DestroyHighlighterView();
    UpdateHighlighterView(current_window, event->root_location_f(), event);
  }

  if (event->type() == ui::ET_TOUCH_MOVED && highlighter_view_)
    UpdateHighlighterView(current_window, event->root_location_f(), event);

  if (event->type() == ui::ET_TOUCH_RELEASED && highlighter_view_) {
    const FastInkPoints& points = highlighter_view_->points();
    const gfx::Rect box = points.GetBoundingBox();
    const gfx::Point pivot(box.CenterPoint());

    const float scale_factor = current_window->layer()->device_scale_factor();

    if (DetectHorizontalStroke(box, HighlighterView::kPenTipSize)) {
      const gfx::Rect box_adjusted =
          AdjustHorizontalStroke(box, HighlighterView::kPenTipSize);
      observer_->HandleSelection(
          gfx::ScaleToEnclosingRect(box_adjusted, scale_factor));
      highlighter_view_->Animate(pivot,
                                 HighlighterView::AnimationMode::kFadeout);

      result_view_ = base::MakeUnique<HighlighterResultView>(current_window);
      result_view_->AnimateInPlace(box_adjusted, HighlighterView::kPenColor);
    } else if (DetectClosedShape(box, points)) {
      observer_->HandleSelection(gfx::ScaleToEnclosingRect(box, scale_factor));
      highlighter_view_->Animate(pivot,
                                 HighlighterView::AnimationMode::kInflate);

      result_view_ = base::MakeUnique<HighlighterResultView>(current_window);
      result_view_->AnimateDeflate(box);
    } else {
      highlighter_view_->Animate(pivot,
                                 HighlighterView::AnimationMode::kDeflate);
    }
  }
}

void HighlighterController::OnWindowDestroying(aura::Window* window) {
  SwitchTargetRootWindowIfNeeded(window);
}

void HighlighterController::SwitchTargetRootWindowIfNeeded(
    aura::Window* root_window) {
  if (!root_window) {
    DestroyHighlighterView();
  }

  if (root_window && observer_ && !highlighter_view_) {
    highlighter_view_ =
        base::MakeUnique<HighlighterView>(presentation_delay_, root_window);
  }
}

void HighlighterController::UpdateHighlighterView(
    aura::Window* current_window,
    const gfx::PointF& event_location,
    ui::Event* event) {
  SwitchTargetRootWindowIfNeeded(current_window);
  DCHECK(highlighter_view_);
  highlighter_view_->AddNewPoint(event_location, event->time_stamp());
  event->StopPropagation();
}

void HighlighterController::DestroyHighlighterView() {
  highlighter_view_.reset();
  result_view_.reset();
}

}  // namespace ash
