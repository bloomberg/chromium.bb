// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_uma.h"

#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"

namespace {

std::string FindAppropriateHistogramNameFromTarget(aura::Window* window,
                                                   const gfx::Point& location) {
  std::string name = window ? window->name() : std::string();
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    views::View* view =
        widget->GetRootView()->GetEventHandlerForPoint(location);
    if (view)
      name = view->GetClassName();
  }

  if (name.empty())
    name = "[unknown]";
  return name;
}

std::vector<int> GetEventCodes() {
  // NOTE: Add new events only at the end of this list. Also, make sure the enum
  // list in tools/histogram/histograms.xml is also updated.
  int types[] = {
    ui::ET_UNKNOWN,         // This is to make sure that every intersting value
                            // has positive index.
    ui::ET_TOUCH_RELEASED,
    ui::ET_TOUCH_PRESSED,
    ui::ET_TOUCH_MOVED,
    ui::ET_TOUCH_STATIONARY,
    ui::ET_TOUCH_CANCELLED,
    ui::ET_GESTURE_SCROLL_BEGIN,
    ui::ET_GESTURE_SCROLL_END,
    ui::ET_GESTURE_SCROLL_UPDATE,
    ui::ET_GESTURE_TAP,
    ui::ET_GESTURE_TAP_DOWN,
    ui::ET_GESTURE_BEGIN,
    ui::ET_GESTURE_END,
    ui::ET_GESTURE_DOUBLE_TAP,
    ui::ET_GESTURE_TWO_FINGER_TAP,
    ui::ET_GESTURE_PINCH_BEGIN,
    ui::ET_GESTURE_PINCH_END,
    ui::ET_GESTURE_PINCH_UPDATE,
    ui::ET_GESTURE_LONG_PRESS,
    ui::ET_GESTURE_MULTIFINGER_SWIPE,
    ui::ET_SCROLL,
    ui::ET_SCROLL_FLING_START,
    ui::ET_SCROLL_FLING_CANCEL,
  };

  return std::vector<int>(types, types + arraysize(types));
}

}

namespace ash {
namespace internal {

TouchUMA::TouchUMA() {
  std::vector<int> types = GetEventCodes();
  for (std::vector<int>::iterator iter = types.begin();
       iter != types.end();
       ++iter) {
    ui_event_type_map_[static_cast<ui::EventType>(*iter)] =
        iter - types.begin();
  }
}

TouchUMA::~TouchUMA() {
}

void TouchUMA::RecordGestureEvent(aura::Window* target,
                                  const aura::GestureEvent& event) {
  UMA_HISTOGRAM_ENUMERATION("Ash.GestureCreated",
      ui_event_type_map_[event.type()],
      ui_event_type_map_.size());

  // Try to record the component the gesture was created on.
  std::string component = FindAppropriateHistogramNameFromTarget(target,
      event.location());
  base::Histogram* histogram = base::LinearHistogram::FactoryGet(
      StringPrintf("Ash.GestureTarget.%s", component.c_str()),
      1, ui_event_type_map_.size(), ui_event_type_map_.size() + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram->Add(ui_event_type_map_[event.type()]);
}

void TouchUMA::RecordTouchEvent(aura::Window* target,
                                const aura::TouchEvent& event) {
  // TODO(sad|rjkroege): Figure out what to do (heat map?).
}

}  // namespace internal
}  // namespace ash
