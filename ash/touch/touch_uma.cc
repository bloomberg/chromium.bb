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

enum UMAEventType {
  UMA_ET_UNKNOWN,
  UMA_ET_TOUCH_RELEASED,
  UMA_ET_TOUCH_PRESSED,
  UMA_ET_TOUCH_MOVED,
  UMA_ET_TOUCH_STATIONARY,
  UMA_ET_TOUCH_CANCELLED,
  UMA_ET_GESTURE_SCROLL_BEGIN,
  UMA_ET_GESTURE_SCROLL_END,
  UMA_ET_GESTURE_SCROLL_UPDATE,
  UMA_ET_GESTURE_TAP,
  UMA_ET_GESTURE_TAP_DOWN,
  UMA_ET_GESTURE_BEGIN,
  UMA_ET_GESTURE_END,
  UMA_ET_GESTURE_DOUBLE_TAP,
  UMA_ET_GESTURE_TWO_FINGER_TAP,
  UMA_ET_GESTURE_PINCH_BEGIN,
  UMA_ET_GESTURE_PINCH_END,
  UMA_ET_GESTURE_PINCH_UPDATE,
  UMA_ET_GESTURE_LONG_PRESS,
  UMA_ET_GESTURE_MULTIFINGER_SWIPE,
  UMA_ET_SCROLL,
  UMA_ET_SCROLL_FLING_START,
  UMA_ET_SCROLL_FLING_CANCEL,
  UMA_ET_GESTURE_MULTIFINGER_SWIPE_3,
  UMA_ET_GESTURE_MULTIFINGER_SWIPE_4P,  // 4+ fingers
  UMA_ET_GESTURE_SCROLL_UPDATE_2,
  UMA_ET_GESTURE_SCROLL_UPDATE_3,
  UMA_ET_GESTURE_SCROLL_UPDATE_4P,
  UMA_ET_GESTURE_PINCH_UPDATE_3,
  UMA_ET_GESTURE_PINCH_UPDATE_4P,
  // NOTE: Add new event types only immediately above this line. Make sure to
  // update the enum list in tools/histogram/histograms.xml accordingly.
  UMA_ET_COUNT
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

UMAEventType UMAEventTypeFromEvent(const aura::Event& event) {
  switch (event.type()) {
    case ui::ET_TOUCH_RELEASED:
      return UMA_ET_TOUCH_RELEASED;
    case ui::ET_TOUCH_PRESSED:
      return UMA_ET_TOUCH_PRESSED;
    case ui::ET_TOUCH_MOVED:
      return UMA_ET_TOUCH_MOVED;
    case ui::ET_TOUCH_STATIONARY:
      return UMA_ET_TOUCH_STATIONARY;
    case ui::ET_TOUCH_CANCELLED:
      return UMA_ET_TOUCH_CANCELLED;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return UMA_ET_GESTURE_SCROLL_BEGIN;
    case ui::ET_GESTURE_SCROLL_END:
      return UMA_ET_GESTURE_SCROLL_END;
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      const aura::GestureEvent& gesture =
          static_cast<const aura::GestureEvent&>(event);
      if (gesture.details().touch_points() >= 4)
        return UMA_ET_GESTURE_SCROLL_UPDATE_4P;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_SCROLL_UPDATE_3;
      else if (gesture.details().touch_points() == 2)
        return UMA_ET_GESTURE_SCROLL_UPDATE_2;
      return UMA_ET_GESTURE_SCROLL_UPDATE;
    }
    case ui::ET_GESTURE_TAP:
      return UMA_ET_GESTURE_TAP;
    case ui::ET_GESTURE_TAP_DOWN:
      return UMA_ET_GESTURE_TAP_DOWN;
    case ui::ET_GESTURE_BEGIN:
      return UMA_ET_GESTURE_BEGIN;
    case ui::ET_GESTURE_END:
      return UMA_ET_GESTURE_END;
    case ui::ET_GESTURE_DOUBLE_TAP:
      return UMA_ET_GESTURE_DOUBLE_TAP;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      return UMA_ET_GESTURE_TWO_FINGER_TAP;
    case ui::ET_GESTURE_PINCH_BEGIN:
      return UMA_ET_GESTURE_PINCH_BEGIN;
    case ui::ET_GESTURE_PINCH_END:
      return UMA_ET_GESTURE_PINCH_END;
    case ui::ET_GESTURE_PINCH_UPDATE: {
      const aura::GestureEvent& gesture =
          static_cast<const aura::GestureEvent&>(event);
      if (gesture.details().touch_points() >= 4)
        return UMA_ET_GESTURE_PINCH_UPDATE_4P;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_PINCH_UPDATE_3;
      return UMA_ET_GESTURE_PINCH_UPDATE;
    }
    case ui::ET_GESTURE_LONG_PRESS:
      return UMA_ET_GESTURE_LONG_PRESS;
    case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
      const aura::GestureEvent& gesture =
          static_cast<const aura::GestureEvent&>(event);
      if (gesture.details().touch_points() >= 4)
        return UMA_ET_GESTURE_MULTIFINGER_SWIPE_4P;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_MULTIFINGER_SWIPE_3;
      return UMA_ET_GESTURE_MULTIFINGER_SWIPE;
    }
    case ui::ET_SCROLL:
      return UMA_ET_SCROLL;
    case ui::ET_SCROLL_FLING_START:
      return UMA_ET_SCROLL_FLING_START;
    case ui::ET_SCROLL_FLING_CANCEL:
      return UMA_ET_SCROLL_FLING_CANCEL;
    default:
      return UMA_ET_UNKNOWN;
  }
}

}

namespace ash {
namespace internal {

TouchUMA::TouchUMA() {
}

TouchUMA::~TouchUMA() {
}

void TouchUMA::RecordGestureEvent(aura::Window* target,
                                  const aura::GestureEvent& event) {
  UMA_HISTOGRAM_ENUMERATION("Ash.GestureCreated",
                            UMAEventTypeFromEvent(event),
                            UMA_ET_COUNT);

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
