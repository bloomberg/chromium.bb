// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_uma.h"

#include "ash/shell_delegate.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/gfx/point_conversions.h"

#if defined(USE_XI2_MT)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#endif

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
  GESTURE_WEBPAGE_SCROLL,
  GESTURE_WEBPAGE_TAP,
  GESTURE_TABSTRIP_TAP,
  GESTURE_BEZEL_DOWN,
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
  UMA_ET_GESTURE_LONG_TAP,
  // NOTE: Add new event types only immediately above this line. Make sure to
  // update the enum list in tools/histogram/histograms.xml accordingly.
  UMA_ET_COUNT
};

struct WindowTouchDetails {
  // Move and start times of the touch points. The key is the touch-id.
  std::map<int, base::TimeDelta> last_move_time_;
  std::map<int, base::TimeDelta> last_start_time_;

  // The first and last positions of the touch points.
  std::map<int, gfx::Point> start_touch_position_;
  std::map<int, gfx::Point> last_touch_position_;

  // Last time-stamp of the last touch-end event.
  base::TimeDelta last_release_time_;

  // Stores the time of the last touch released on this window (if there was a
  // multi-touch gesture on the window, then this is the release-time of the
  // last touch on the window).
  base::TimeDelta last_mt_time_;
};

DEFINE_OWNED_WINDOW_PROPERTY_KEY(WindowTouchDetails,
                                 kWindowTouchDetails,
                                 NULL);

GestureActionType FindGestureActionType(aura::Window* window,
                                        const ui::GestureEvent& event) {
  if (!window || window->GetRootWindow() == window) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_BEZEL_SCROLL;
    if (event.type() == ui::ET_GESTURE_BEGIN)
      return GESTURE_BEZEL_DOWN;
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
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_WEBPAGE_SCROLL;
    if (event.type() == ui::ET_GESTURE_TAP)
      return GESTURE_WEBPAGE_TAP;
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
    if (event.type() == ui::ET_GESTURE_TAP)
      return GESTURE_TABSTRIP_TAP;
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

UMAEventType UMAEventTypeFromEvent(const ui::Event& event) {
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
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
      if (gesture.details().touch_points() >= 4)
        return UMA_ET_GESTURE_SCROLL_UPDATE_4P;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_SCROLL_UPDATE_3;
      else if (gesture.details().touch_points() == 2)
        return UMA_ET_GESTURE_SCROLL_UPDATE_2;
      return UMA_ET_GESTURE_SCROLL_UPDATE;
    }
    case ui::ET_GESTURE_TAP: {
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
      int tap_count = gesture.details().tap_count();
      if (tap_count == 1)
        return UMA_ET_GESTURE_TAP;
      else if (tap_count == 2)
        return UMA_ET_GESTURE_DOUBLE_TAP;
      NOTREACHED() << "Received tap with tapcount " << tap_count;
      return UMA_ET_UNKNOWN;
    }
    case ui::ET_GESTURE_TAP_DOWN:
      return UMA_ET_GESTURE_TAP_DOWN;
    case ui::ET_GESTURE_BEGIN:
      return UMA_ET_GESTURE_BEGIN;
    case ui::ET_GESTURE_END:
      return UMA_ET_GESTURE_END;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      return UMA_ET_GESTURE_TWO_FINGER_TAP;
    case ui::ET_GESTURE_PINCH_BEGIN:
      return UMA_ET_GESTURE_PINCH_BEGIN;
    case ui::ET_GESTURE_PINCH_END:
      return UMA_ET_GESTURE_PINCH_END;
    case ui::ET_GESTURE_PINCH_UPDATE: {
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
      if (gesture.details().touch_points() >= 4)
        return UMA_ET_GESTURE_PINCH_UPDATE_4P;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_PINCH_UPDATE_3;
      return UMA_ET_GESTURE_PINCH_UPDATE;
    }
    case ui::ET_GESTURE_LONG_PRESS:
      return UMA_ET_GESTURE_LONG_PRESS;
    case ui::ET_GESTURE_LONG_TAP:
      return UMA_ET_GESTURE_LONG_TAP;
    case ui::ET_GESTURE_MULTIFINGER_SWIPE: {
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
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

TouchUMA::TouchUMA()
    : touch_in_progress_(false),
      burst_length_(0) {
}

TouchUMA::~TouchUMA() {
}

void TouchUMA::RecordGestureEvent(aura::Window* target,
                                  const ui::GestureEvent& event) {
  UMA_HISTOGRAM_ENUMERATION("Ash.GestureCreated",
                            UMAEventTypeFromEvent(event),
                            UMA_ET_COUNT);

  GestureActionType action = FindGestureActionType(target, event);
  if (action != GESTURE_UNKNOWN) {
    UMA_HISTOGRAM_ENUMERATION("Ash.GestureTarget",
                              action,
                              GESTURE_ACTION_COUNT);
  }

  if (event.type() == ui::ET_GESTURE_END &&
      event.details().touch_points() == 2) {
    WindowTouchDetails* details = target->GetProperty(kWindowTouchDetails);
    if (!details) {
      LOG(ERROR) << "Window received gesture events without receiving any touch"
                    " events";
      return;
    }
    details->last_mt_time_ = event.time_stamp();
  }
}

void TouchUMA::RecordTouchEvent(aura::Window* target,
                                const ui::TouchEvent& event) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchRadius",
      static_cast<int>(std::max(event.radius_x(), event.radius_y())),
      1, 500, 100);

  UpdateBurstData(event);

  WindowTouchDetails* details = target->GetProperty(kWindowTouchDetails);
  if (!details) {
    details = new WindowTouchDetails;
    target->SetProperty(kWindowTouchDetails, details);
  }

  // Record the location of the touch points.
  const int kBucketCountForLocation = 100;
  const gfx::Rect bounds = target->GetRootWindow()->bounds();
  const int bucket_size_x = bounds.width() / kBucketCountForLocation;
  const int bucket_size_y = bounds.height() / kBucketCountForLocation;

  gfx::Point position = event.root_location();

  // Prefer raw event location (when available) over calibrated location.
  if (event.HasNativeEvent()) {
#if defined(USE_XI2_MT)
    XEvent* xevent = event.native_event();
    CHECK_EQ(GenericEvent, xevent->type);
    XIEvent* xievent = static_cast<XIEvent*>(xevent->xcookie.data);
    if (xievent->evtype == XI_TouchBegin ||
        xievent->evtype == XI_TouchUpdate ||
        xievent->evtype == XI_TouchEnd) {
      XIDeviceEvent* device_event =
          static_cast<XIDeviceEvent*>(xevent->xcookie.data);
      position.SetPoint(static_cast<int>(device_event->event_x),
                        static_cast<int>(device_event->event_y));
    } else {
      position = ui::EventLocationFromNative(event.native_event());
    }
#else
    position = ui::EventLocationFromNative(event.native_event());
#endif
    position = gfx::ToFlooredPoint(
        gfx::ScalePoint(position, 1. / target->layer()->device_scale_factor()));
  }

  position.set_x(std::min(bounds.width() - 1, std::max(0, position.x())));
  position.set_y(std::min(bounds.height() - 1, std::max(0, position.y())));

  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchPositionX",
      position.x() / bucket_size_x,
      0, kBucketCountForLocation, kBucketCountForLocation + 1);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchPositionY",
      position.y() / bucket_size_y,
      0, kBucketCountForLocation, kBucketCountForLocation + 1);

  if (event.type() == ui::ET_TOUCH_PRESSED) {
    Shell::GetInstance()->delegate()->RecordUserMetricsAction(
        UMA_TOUCHSCREEN_TAP_DOWN);

    details->last_start_time_[event.touch_id()] = event.time_stamp();
    details->start_touch_position_[event.touch_id()] = event.root_location();
    details->last_touch_position_[event.touch_id()] = event.location();

    if (details->last_release_time_.ToInternalValue()) {
      // Measuring the interval between a touch-release and the next
      // touch-start is probably less useful when doing multi-touch (e.g.
      // gestures, or multi-touch friendly apps). So count this only if the user
      // hasn't done any multi-touch during the last 30 seconds.
      base::TimeDelta diff = event.time_stamp() - details->last_mt_time_;
      if (diff.InSeconds() > 30) {
        base::TimeDelta gap = event.time_stamp() - details->last_release_time_;
        UMA_HISTOGRAM_COUNTS_10000("Ash.TouchStartAfterEnd",
            gap.InMilliseconds());
      }
    }

    // Record the number of touch-points currently active for the window.
    const int kMaxTouchPoints = 10;
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.ActiveTouchPoints",
        details->last_start_time_.size(),
        1, kMaxTouchPoints, kMaxTouchPoints + 1);
  } else if (event.type() == ui::ET_TOUCH_RELEASED) {
    if (details->last_start_time_.count(event.touch_id())) {
      base::TimeDelta duration = event.time_stamp() -
                                 details->last_start_time_[event.touch_id()];
      UMA_HISTOGRAM_COUNTS_100("Ash.TouchDuration", duration.InMilliseconds());

      // Look for touches that were [almost] stationary for a long time.
      const double kLongStationaryTouchDuration = 10;
      const int kLongStationaryTouchDistanceSquared = 100;
      if (duration.InSecondsF() > kLongStationaryTouchDuration) {
        gfx::Vector2d distance = event.root_location() -
            details->start_touch_position_[event.touch_id()];
        if (distance.LengthSquared() < kLongStationaryTouchDistanceSquared) {
          UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.StationaryTouchDuration",
              duration.InSeconds(),
              kLongStationaryTouchDuration,
              1000,
              20);
        }
      }
    }
    details->last_start_time_.erase(event.touch_id());
    details->last_move_time_.erase(event.touch_id());
    details->start_touch_position_.erase(event.touch_id());
    details->last_touch_position_.erase(event.touch_id());
    details->last_release_time_ = event.time_stamp();
  } else if (event.type() == ui::ET_TOUCH_MOVED) {
    int distance = 0;
    if (details->last_touch_position_.count(event.touch_id())) {
      gfx::Point lastpos = details->last_touch_position_[event.touch_id()];
      distance = abs(lastpos.x() - event.x()) + abs(lastpos.y() - event.y());
    }

    if (details->last_move_time_.count(event.touch_id())) {
      base::TimeDelta move_delay = event.time_stamp() -
                                   details->last_move_time_[event.touch_id()];
      UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchMoveInterval",
                                  move_delay.InMilliseconds(),
                                  1, 50, 25);
    }

    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchMoveSteps", distance, 1, 1000, 50);

    details->last_move_time_[event.touch_id()] = event.time_stamp();
    details->last_touch_position_[event.touch_id()] = event.location();
  }
}

void TouchUMA::UpdateBurstData(const ui::TouchEvent& event) {
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    if (!touch_in_progress_) {
      base::TimeDelta difference = event.time_stamp() - last_touch_down_time_;
      if (difference > base::TimeDelta::FromMilliseconds(250)) {
        if (burst_length_) {
          UMA_HISTOGRAM_COUNTS_100("Ash.TouchStartBurst",
                                   std::min(burst_length_, 100));
        }
        burst_length_ = 1;
      } else {
        ++burst_length_;
      }
    }
    touch_in_progress_ = true;
    last_touch_down_time_ = event.time_stamp();
  } else if (event.type() == ui::ET_TOUCH_RELEASED) {
    if (!aura::Env::GetInstance()->is_touch_down())
      touch_in_progress_ = false;
  }
}

}  // namespace internal
}  // namespace ash
