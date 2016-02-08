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
#include "ui/gfx/geometry/point_conversions.h"

#if defined(USE_X11)
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>
#endif

namespace {

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
}

DECLARE_WINDOW_PROPERTY_TYPE(WindowTouchDetails*);

namespace ash {

// static
TouchUMA* TouchUMA::GetInstance() {
  return base::Singleton<TouchUMA>::get();
}

void TouchUMA::RecordGestureEvent(aura::Window* target,
                                  const ui::GestureEvent& event) {
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
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Ash.TouchRadius",
      static_cast<int>(std::max(event.pointer_details().radius_x,
                                event.pointer_details().radius_y)),
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
    position = ui::EventLocationFromNative(event.native_event());
    position = gfx::ScaleToFlooredPoint(
        position, 1.f / target->layer()->device_scale_factor());
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

  // |widget| may be in the process of destroying if it has ownership
  // views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET and |event| was
  // dispatched as part of gesture state cleanup. In this case the RootView
  // of |widget| may no longer exist, so check before calling into any
  // RootView methods.
  if (!widget->GetRootView())
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
