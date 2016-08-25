// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stylus_metrics_recorder.h"

#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/metrics/histogram.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace ash {

// Form factor of the down event. This enum is used to back an UMA histogram
// and should be treated as append-only.
enum DownEventFormFactor {
  DOWN_EVENT_FORMFACTOR_CLAMSHELL = 0,
  DOWN_EVENT_FORMFACTOR_TOUCHVIEW,
  DOWN_EVENT_FORMFACTOR_COUNT
};

// Input type of the down event. This enum is used to back an UMA histogram
// and should be treated as append-only.
enum DownEventSource {
  DOWN_EVENT_SOURCE_UNKNOWN = 0,
  DOWN_EVENT_SOURCE_MOUSE,
  DOWN_EVENT_SOURCE_STYLUS,
  DOWN_EVENT_SOURCE_TOUCH,
  DOWN_EVENT_SOURCE_COUNT
};

StylusMetricsRecorder::StylusMetricsRecorder() {}

StylusMetricsRecorder::~StylusMetricsRecorder() {}

void StylusMetricsRecorder::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return;
  RecordUMA(event->pointer_details().pointer_type);
}

void StylusMetricsRecorder::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;
  RecordUMA(event->pointer_details().pointer_type);
}

void StylusMetricsRecorder::RecordUMA(ui::EventPointerType type) {
  DownEventFormFactor form_factor = DOWN_EVENT_FORMFACTOR_CLAMSHELL;
  if (WmShell::Get()
          ->maximize_mode_controller()
          ->IsMaximizeModeWindowManagerEnabled()) {
    form_factor = DOWN_EVENT_FORMFACTOR_TOUCHVIEW;
  }
  UMA_HISTOGRAM_ENUMERATION("Event.DownEventCount.PerFormFactor", form_factor,
                            DOWN_EVENT_FORMFACTOR_COUNT);

  DownEventSource input_type = DOWN_EVENT_SOURCE_UNKNOWN;
  switch (type) {
    case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
      input_type = DOWN_EVENT_SOURCE_UNKNOWN;
      break;
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      input_type = DOWN_EVENT_SOURCE_MOUSE;
      break;
    case ui::EventPointerType::POINTER_TYPE_PEN:
      input_type = DOWN_EVENT_SOURCE_STYLUS;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      input_type = DOWN_EVENT_SOURCE_TOUCH;
      break;
    case ui::EventPointerType::POINTER_TYPE_ERASER:
      input_type = DOWN_EVENT_SOURCE_STYLUS;
      break;
  }

  UMA_HISTOGRAM_ENUMERATION("Event.DownEventCount.PerInput", input_type,
                            DOWN_EVENT_SOURCE_COUNT);
}

}  // namespace ash
