// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button_pressed_metric_tracker.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "ui/views/controls/button/button.h"

namespace ash {

const char ShelfButtonPressedMetricTracker::
    kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName[] =
        "Ash.Shelf.TimeBetweenWindowMinimizedAndActivatedActions";

ShelfButtonPressedMetricTracker::ShelfButtonPressedMetricTracker()
    : tick_clock_(new base::DefaultTickClock()),
      time_of_last_minimize_(base::TimeTicks()),
      last_minimized_source_button_(nullptr) {
}

ShelfButtonPressedMetricTracker::~ShelfButtonPressedMetricTracker() {
}

void ShelfButtonPressedMetricTracker::ButtonPressed(
    const ui::Event& event,
    const views::Button* sender,
    ShelfItemDelegate::PerformedAction performed_action) {
  RecordButtonPressedSource(event);
  RecordButtonPressedAction(performed_action);

  switch (performed_action) {
    case ShelfItemDelegate::kExistingWindowMinimized:
      SetMinimizedData(sender);
      break;
    case ShelfItemDelegate::kExistingWindowActivated:
      if (IsSubsequentActivationEvent(sender))
        RecordTimeBetweenMinimizedAndActivated();
      break;
    default:
      break;
  }

  if (performed_action != ShelfItemDelegate::kExistingWindowMinimized)
    ResetMinimizedData();
}

void ShelfButtonPressedMetricTracker::RecordButtonPressedSource(
    const ui::Event& event) {
  if (event.IsMouseEvent()) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        UMA_LAUNCHER_BUTTON_PRESSED_WITH_MOUSE);
  } else if (event.IsGestureEvent()) {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        UMA_LAUNCHER_BUTTON_PRESSED_WITH_TOUCH);
  }
}

void ShelfButtonPressedMetricTracker::RecordButtonPressedAction(
    ShelfItemDelegate::PerformedAction performed_action) {
  switch (performed_action) {
    case ShelfItemDelegate::kNoAction:
    case ShelfItemDelegate::kAppListMenuShown:
      break;
    case ShelfItemDelegate::kNewWindowCreated:
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          UMA_LAUNCHER_LAUNCH_TASK);
      break;
    case ShelfItemDelegate::kExistingWindowActivated:
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          UMA_LAUNCHER_SWITCH_TASK);
      break;
    case ShelfItemDelegate::kExistingWindowMinimized:
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          UMA_LAUNCHER_MINIMIZE_TASK);
      break;
  }
}

void ShelfButtonPressedMetricTracker::RecordTimeBetweenMinimizedAndActivated() {
  UMA_HISTOGRAM_TIMES(
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName,
      tick_clock_->NowTicks() - time_of_last_minimize_);
}

bool ShelfButtonPressedMetricTracker::IsSubsequentActivationEvent(
    const views::Button* sender) const {
  return time_of_last_minimize_ != base::TimeTicks() &&
         last_minimized_source_button_ == sender;
}

void ShelfButtonPressedMetricTracker::SetMinimizedData(
    const views::Button* sender) {
  last_minimized_source_button_ = sender;
  time_of_last_minimize_ = tick_clock_->NowTicks();
}

void ShelfButtonPressedMetricTracker::ResetMinimizedData() {
  last_minimized_source_button_ = nullptr;
  time_of_last_minimize_ = base::TimeTicks();
}

}  // namespace ash
