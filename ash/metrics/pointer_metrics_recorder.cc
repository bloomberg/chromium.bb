// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/pointer_metrics_recorder.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/app_types.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

int GetDestination(views::Widget* target) {
  if (!target)
    return static_cast<int>(AppType::OTHERS);

  aura::Window* window = target->GetNativeWindow();
  DCHECK(window);
  return window->GetProperty(aura::client::kAppType);
}

// Find the input type, form factor and destination combination of the down
// event. Used to get the UMA histogram bucket.
DownEventMetric FindCombination(DownEventSource input_type,
                                DownEventFormFactor form_factor,
                                int destination) {
  int num_combination_per_input =
      kAppCount * static_cast<int>(DownEventFormFactor::kFormFactorCount);
  int result = static_cast<int>(input_type) * num_combination_per_input +
               static_cast<int>(form_factor) * kAppCount + destination;
  DCHECK(result >= 0 &&
         result < static_cast<int>(DownEventMetric::kCombinationCount));
  return static_cast<DownEventMetric>(result);
}

void RecordUMA(ui::EventPointerType type, views::Widget* target) {
  DownEventFormFactor form_factor = DownEventFormFactor::kClamshell;
  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled()) {
    OrientationLockType screen_orientation =
        Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
    if (screen_orientation == OrientationLockType::kLandscapePrimary ||
        screen_orientation == OrientationLockType::kLandscapeSecondary) {
      form_factor = DownEventFormFactor::kTabletModeLandscape;
    } else {
      form_factor = DownEventFormFactor::kTabletModePortrait;
    }
  }

  DownEventSource input_type = DownEventSource::kUnknown;
  switch (type) {
    case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
      input_type = DownEventSource::kUnknown;
      break;
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      input_type = DownEventSource::kMouse;
      break;
    case ui::EventPointerType::POINTER_TYPE_PEN:
      input_type = DownEventSource::kStylus;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      input_type = DownEventSource::kTouch;
      break;
    case ui::EventPointerType::POINTER_TYPE_ERASER:
      input_type = DownEventSource::kStylus;
      break;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Event.DownEventCount.PerInputFormFactorDestinationCombination",
      FindCombination(input_type, form_factor, GetDestination(target)),
      DownEventMetric::kCombinationCount);
}

}  // namespace

PointerMetricsRecorder::PointerMetricsRecorder() {
  ShellPort::Get()->AddPointerWatcher(this,
                                      views::PointerWatcherEventTypes::BASIC);
}

PointerMetricsRecorder::~PointerMetricsRecorder() {
  ShellPort::Get()->RemovePointerWatcher(this);
}

void PointerMetricsRecorder::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    gfx::NativeView target) {
  if (event.type() == ui::ET_POINTER_DOWN)
    RecordUMA(event.pointer_details().pointer_type,
              views::Widget::GetTopLevelWidgetForNativeView(target));
}

}  // namespace ash
