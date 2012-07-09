// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_uma.h"

#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

namespace {

enum GestureActionType {
  GESTURE_UNKNOWN,
  GESTURE_OMNIBOX_PINCH,
  GESTURE_OMNIBOX_SCROLL,
  GESTURE_TABSTRIP_PINCH,
  GESTURE_TABSTRIP_SCROLL,
  GESTURE_BEZEL_SCROLL,
  GESTURE_DESKTOP_SCROLL,
  GESTURE_DESKTOP_PINCH,
  GESTURE_WEBPAGE_PINCH,
// NOTE: Add new action types only immediately above this line. Also, make sure
// the enum list in tools/histogram/histograms.xml is updated with any change in
// here.
  GESTURE_ACTION_COUNT
};

GestureActionType FindGestureActionType(aura::Window* window,
                                        const aura::GestureEvent& event) {
  if (!window || window->GetRootWindow() == window) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_BEZEL_SCROLL;
    return GESTURE_UNKNOWN;
  }

  std::string name = window ? window->name() : std::string();

  const char kDesktopBackgroundView[] = "DesktopBackgroundView";
  if (name == kDesktopBackgroundView) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_DESKTOP_SCROLL;
    if (event.type() == ui::ET_GESTURE_PINCH_BEGIN)
      return GESTURE_DESKTOP_PINCH;
    return GESTURE_UNKNOWN;
  }

  const char kWebPage[] = "RenderWidgetHostViewAura";
  if (name == kWebPage) {
    if (event.type() == ui::ET_GESTURE_PINCH_BEGIN)
      return GESTURE_WEBPAGE_PINCH;
    return GESTURE_UNKNOWN;
  }

  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (!widget)
    return GESTURE_UNKNOWN;

  views::View* view = widget->GetRootView()->
      GetEventHandlerForPoint(event.location());
  if (!view)
    return GESTURE_UNKNOWN;

  name = view->GetClassName();

  const char kTabStrip[] = "TabStrip";
  const char kTab[] = "BrowserTab";
  if (name == kTabStrip || name == kTab) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_TABSTRIP_SCROLL;
    if (event.type() == ui::ET_GESTURE_PINCH_BEGIN)
      return GESTURE_TABSTRIP_PINCH;
    return GESTURE_UNKNOWN;
  }

  const char kOmnibox[] = "BrowserOmniboxViewViews";
  if (name == kOmnibox) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_OMNIBOX_SCROLL;
    if (event.type() == ui::ET_GESTURE_PINCH_BEGIN)
      return GESTURE_OMNIBOX_PINCH;
    return GESTURE_UNKNOWN;
  }

  return GESTURE_UNKNOWN;
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

  GestureActionType action = FindGestureActionType(target, event);
  if (action != GESTURE_UNKNOWN) {
    UMA_HISTOGRAM_ENUMERATION("Ash.GestureTarget",
                              action,
                              GESTURE_ACTION_COUNT);
  }
}

void TouchUMA::RecordTouchEvent(aura::Window* target,
                                const aura::TouchEvent& event) {
  // TODO(sad|rjkroege): Figure out what to do (heat map?).
}

}  // namespace internal
}  // namespace ash
