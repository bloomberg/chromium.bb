// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_uma.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/point_conversions.h"

#if defined(USE_XI2_MT)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#endif

namespace {

enum UMAEventType {
  // WARNING: Do not change the numerical values of any of these types.
  // Do not remove deprecated types - just comment them as deprecated.
  UMA_ET_UNKNOWN = 0,
  UMA_ET_TOUCH_RELEASED = 1,
  UMA_ET_TOUCH_PRESSED = 2,
  UMA_ET_TOUCH_MOVED = 3,
  UMA_ET_TOUCH_STATIONARY = 4,  // Deprecated. Do not remove.
  UMA_ET_TOUCH_CANCELLED = 5,
  UMA_ET_GESTURE_SCROLL_BEGIN = 6,
  UMA_ET_GESTURE_SCROLL_END = 7,
  UMA_ET_GESTURE_SCROLL_UPDATE = 8,
  UMA_ET_GESTURE_TAP = 9,
  UMA_ET_GESTURE_TAP_DOWN = 10,
  UMA_ET_GESTURE_BEGIN = 11,
  UMA_ET_GESTURE_END = 12,
  UMA_ET_GESTURE_DOUBLE_TAP = 13,
  UMA_ET_GESTURE_TRIPLE_TAP = 14,
  UMA_ET_GESTURE_TWO_FINGER_TAP = 15,
  UMA_ET_GESTURE_PINCH_BEGIN = 16,
  UMA_ET_GESTURE_PINCH_END = 17,
  UMA_ET_GESTURE_PINCH_UPDATE = 18,
  UMA_ET_GESTURE_LONG_PRESS = 19,
  UMA_ET_GESTURE_SWIPE_2 = 20,   // Swipe with 2 fingers
  UMA_ET_SCROLL = 21,
  UMA_ET_SCROLL_FLING_START = 22,
  UMA_ET_SCROLL_FLING_CANCEL = 23,
  UMA_ET_GESTURE_SWIPE_3 = 24,   // Swipe with 3 fingers
  UMA_ET_GESTURE_SWIPE_4P = 25,  // Swipe with 4+ fingers
  UMA_ET_GESTURE_SCROLL_UPDATE_2 = 26,
  UMA_ET_GESTURE_SCROLL_UPDATE_3 = 27,
  UMA_ET_GESTURE_SCROLL_UPDATE_4P = 28,
  UMA_ET_GESTURE_PINCH_UPDATE_3 = 29,
  UMA_ET_GESTURE_PINCH_UPDATE_4P = 30,
  UMA_ET_GESTURE_LONG_TAP = 31,
  UMA_ET_GESTURE_SHOW_PRESS = 32,
  UMA_ET_GESTURE_TAP_CANCEL = 33,
  UMA_ET_GESTURE_WIN8_EDGE_SWIPE = 34,
  UMA_ET_GESTURE_SWIPE_1 = 35,   // Swipe with 1 finger
  // NOTE: Add new event types only immediately above this line. Make sure to
  // update the UIEventType enum in tools/metrics/histograms/histograms.xml
  // accordingly.
  UMA_ET_COUNT
};

struct WindowTouchDetails {
  WindowTouchDetails()
      : max_distance_from_start_squared_(0) {
  }

  // Move and start times of the touch points. The key is the touch-id.
  std::map<int, base::TimeDelta> last_move_time_;
  std::map<int, base::TimeDelta> last_start_time_;

  // The first and last positions of the touch points.
  std::map<int, gfx::Point> start_touch_position_;
  std::map<int, gfx::Point> last_touch_position_;

  // The maximum distance the first touch point travelled from its starting
  // location in pixels.
  float max_distance_from_start_squared_;

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


UMAEventType UMAEventTypeFromEvent(const ui::Event& event) {
  switch (event.type()) {
    case ui::ET_TOUCH_RELEASED:
      return UMA_ET_TOUCH_RELEASED;
    case ui::ET_TOUCH_PRESSED:
      return UMA_ET_TOUCH_PRESSED;
    case ui::ET_TOUCH_MOVED:
      return UMA_ET_TOUCH_MOVED;
    case ui::ET_TOUCH_CANCELLED:
      return UMA_ET_TOUCH_CANCELLED;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return UMA_ET_GESTURE_SCROLL_BEGIN;
    case ui::ET_GESTURE_SCROLL_END:
      return UMA_ET_GESTURE_SCROLL_END;
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
      if (gesture.details().touch_points() == 1)
        return UMA_ET_GESTURE_SCROLL_UPDATE;
      else if (gesture.details().touch_points() == 2)
        return UMA_ET_GESTURE_SCROLL_UPDATE_2;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_SCROLL_UPDATE_3;
      return UMA_ET_GESTURE_SCROLL_UPDATE_4P;
    }
    case ui::ET_GESTURE_TAP: {
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
      int tap_count = gesture.details().tap_count();
      if (tap_count == 1)
        return UMA_ET_GESTURE_TAP;
      if (tap_count == 2)
        return UMA_ET_GESTURE_DOUBLE_TAP;
      if (tap_count == 3)
        return UMA_ET_GESTURE_TRIPLE_TAP;
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
    case ui::ET_GESTURE_SWIPE: {
      const ui::GestureEvent& gesture =
          static_cast<const ui::GestureEvent&>(event);
      if (gesture.details().touch_points() == 1)
        return UMA_ET_GESTURE_SWIPE_1;
      else if (gesture.details().touch_points() == 2)
        return UMA_ET_GESTURE_SWIPE_2;
      else if (gesture.details().touch_points() == 3)
        return UMA_ET_GESTURE_SWIPE_3;
      return UMA_ET_GESTURE_SWIPE_4P;
    }
    case ui::ET_GESTURE_WIN8_EDGE_SWIPE:
      return UMA_ET_GESTURE_WIN8_EDGE_SWIPE;
    case ui::ET_GESTURE_TAP_CANCEL:
      return UMA_ET_GESTURE_TAP_CANCEL;
    case ui::ET_GESTURE_SHOW_PRESS:
      return UMA_ET_GESTURE_SHOW_PRESS;
    case ui::ET_SCROLL:
      return UMA_ET_SCROLL;
    case ui::ET_SCROLL_FLING_START:
      return UMA_ET_SCROLL_FLING_START;
    case ui::ET_SCROLL_FLING_CANCEL:
      return UMA_ET_SCROLL_FLING_CANCEL;
    default:
      NOTREACHED();
      return UMA_ET_UNKNOWN;
  }
}

}

namespace ash {

// static
TouchUMA* TouchUMA::GetInstance() {
  return Singleton<TouchUMA>::get();
}

void TouchUMA::RecordGestureEvent(aura::Window* target,
                                  const ui::GestureEvent& event) {
  UMA_HISTOGRAM_ENUMERATION("Ash.GestureCreated",
                            UMAEventTypeFromEvent(event),
                            UMA_ET_COUNT);

  GestureActionType action = FindGestureActionType(target, event);
  RecordGestureAction(action);

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

void TouchUMA::RecordGestureAction(GestureActionType action) {
  if (action == GESTURE_UNKNOWN || action >= GESTURE_ACTION_COUNT)
    return;
  UMA_HISTOGRAM_ENUMERATION("Ash.GestureTarget", action,
                            GESTURE_ACTION_COUNT);
}

void TouchUMA::RecordTouchEvent(aura::Window* target,
                                const ui::TouchEvent& event) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchRadius",
      static_cast<int>(std::max(event.radius_x(), event.radius_y())),
      1, 500, 100);

  UpdateTouchState(event);

  WindowTouchDetails* details = target->GetProperty(kWindowTouchDetails);
  if (!details) {
    details = new WindowTouchDetails;
    target->SetProperty(kWindowTouchDetails, details);
  }

  // Record the location of the touch points.
  const int kBucketCountForLocation = 100;
  const gfx::Rect bounds = target->GetRootWindow()->bounds();
  const int bucket_size_x = std::max(1,
                                     bounds.width() / kBucketCountForLocation);
  const int bucket_size_y = std::max(1,
                                     bounds.height() / kBucketCountForLocation);

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
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        UMA_TOUCHSCREEN_TAP_DOWN);

    details->last_start_time_[event.touch_id()] = event.time_stamp();
    details->start_touch_position_[event.touch_id()] = event.root_location();
    details->last_touch_position_[event.touch_id()] = event.location();
    details->max_distance_from_start_squared_ = 0;

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
    if (is_single_finger_gesture_) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchMaxDistance",
          static_cast<int>(
              sqrt(details->max_distance_from_start_squared_)), 0, 1500, 50);
    }

    if (details->last_start_time_.count(event.touch_id())) {
      base::TimeDelta duration = event.time_stamp() -
                                 details->last_start_time_[event.touch_id()];
      UMA_HISTOGRAM_TIMES("Ash.TouchDuration2", duration);

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
      distance =
          std::abs(lastpos.x() - event.x()) + std::abs(lastpos.y() - event.y());
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

    float cur_dist = (details->start_touch_position_[event.touch_id()] -
                      event.root_location()).LengthSquared();
    if (cur_dist > details->max_distance_from_start_squared_)
      details->max_distance_from_start_squared_ = cur_dist;
  }
}

TouchUMA::TouchUMA()
    : is_single_finger_gesture_(false),
      touch_in_progress_(false),
      burst_length_(0) {
}

TouchUMA::~TouchUMA() {
}

void TouchUMA::UpdateTouchState(const ui::TouchEvent& event) {
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    if (!touch_in_progress_) {
      is_single_finger_gesture_ = true;
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
    } else {
      is_single_finger_gesture_ = false;
    }
    touch_in_progress_ = true;
    last_touch_down_time_ = event.time_stamp();
  } else if (event.type() == ui::ET_TOUCH_RELEASED) {
    if (!aura::Env::GetInstance()->is_touch_down())
      touch_in_progress_ = false;
  }
}

TouchUMA::GestureActionType TouchUMA::FindGestureActionType(
    aura::Window* window,
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

}  // namespace ash
