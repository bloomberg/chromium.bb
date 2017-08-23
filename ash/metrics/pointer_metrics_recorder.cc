// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/pointer_metrics_recorder.h"

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

// Form factor of the down event. This enum is used to back an UMA histogram
// and new values should be inserted immediately above FORM_FACTOR_COUNT.
enum class DownEventFormFactor {
  CLAMSHELL = 0,
  TABLET_MODE,
  FORM_FACTOR_COUNT,
};

// Input type of the down event. This enum is used to back an UMA
// histogram and new values should be inserted immediately above SOURCE_COUNT.
enum class DownEventSource {
  UNKNOWN = 0,
  MOUSE,
  STYLUS,
  TOUCH,
  SOURCE_COUNT,
};

int GetDestination(views::Widget* target) {
  if (!target)
    return static_cast<int>(AppType::OTHERS);

  aura::Window* window = target->GetNativeWindow();
  DCHECK(window);
  return window->GetProperty(aura::client::kAppType);
}

void RecordUMA(ui::EventPointerType type, views::Widget* target) {
  DownEventFormFactor form_factor = DownEventFormFactor::CLAMSHELL;
  if (Shell::Get()
          ->tablet_mode_controller()
          ->IsTabletModeWindowManagerEnabled()) {
    form_factor = DownEventFormFactor::TABLET_MODE;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Event.DownEventCount.PerFormFactor",
      static_cast<base::HistogramBase::Sample>(form_factor),
      static_cast<base::HistogramBase::Sample>(
          DownEventFormFactor::FORM_FACTOR_COUNT));

  DownEventSource input_type = DownEventSource::UNKNOWN;
  switch (type) {
    case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
      input_type = DownEventSource::UNKNOWN;
      break;
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      input_type = DownEventSource::MOUSE;
      break;
    case ui::EventPointerType::POINTER_TYPE_PEN:
      input_type = DownEventSource::STYLUS;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      input_type = DownEventSource::TOUCH;
      break;
    case ui::EventPointerType::POINTER_TYPE_ERASER:
      input_type = DownEventSource::STYLUS;
      break;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Event.DownEventCount.PerInput",
      static_cast<base::HistogramBase::Sample>(input_type),
      static_cast<base::HistogramBase::Sample>(DownEventSource::SOURCE_COUNT));

  UMA_HISTOGRAM_ENUMERATION("Event.DownEventCount.PerDestination",
                            GetDestination(target), kAppCount);
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
