// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"

using blink::WebInputEvent;

namespace content {

namespace {

bool IsPullToRefreshEnabled() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kPullToRefresh) == "1";
}

bool IsGestureEventFromTouchpad(const blink::WebInputEvent& event) {
  DCHECK(blink::WebInputEvent::IsGestureEventType(event.GetType()));
  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  return gesture.source_device == blink::kWebGestureDeviceTouchpad;
}

float ClampAbsoluteValue(float value, float max_abs) {
  DCHECK_LT(0.f, max_abs);
  return std::max(-max_abs, std::min(value, max_abs));
}

}  // namespace

OverscrollController::OverscrollController() {}

OverscrollController::~OverscrollController() {}

bool OverscrollController::ShouldProcessEvent(
    const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::kMouseWheel:
      return false;
    case blink::WebInputEvent::kGestureScrollBegin:
    case blink::WebInputEvent::kGestureScrollUpdate:
    case blink::WebInputEvent::kGestureScrollEnd: {
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);

      // GestureScrollBegin and GestureScrollEnd events are created to wrap
      // individual resent GestureScrollUpdates from a plugin. Hence these
      // should not be used to indicate the beginning/end of the overscroll.
      // TODO(mcnee): When we remove BrowserPlugin, delete this code.
      // See crbug.com/533069
      if (gesture.resending_plugin_id != -1 &&
          event.GetType() != blink::WebInputEvent::kGestureScrollUpdate)
        return false;

      blink::WebGestureEvent::ScrollUnits scrollUnits;
      switch (event.GetType()) {
        case blink::WebInputEvent::kGestureScrollBegin:
          scrollUnits = gesture.data.scroll_begin.delta_hint_units;
          break;
        case blink::WebInputEvent::kGestureScrollUpdate:
          scrollUnits = gesture.data.scroll_update.delta_units;
          break;
        case blink::WebInputEvent::kGestureScrollEnd:
          scrollUnits = gesture.data.scroll_end.delta_units;
          break;
        default:
          scrollUnits = blink::WebGestureEvent::kPixels;
          break;
      }

      return scrollUnits == blink::WebGestureEvent::kPrecisePixels;
    }
    default:
      break;
  }
  return true;
}

bool OverscrollController::WillHandleEvent(const blink::WebInputEvent& event) {
  if (!ShouldProcessEvent(event))
    return false;

  bool reset_scroll_state = false;
  if (scroll_state_ != STATE_UNKNOWN ||
      overscroll_delta_x_ || overscroll_delta_y_) {
    switch (event.GetType()) {
      case blink::WebInputEvent::kGestureScrollEnd:
        // Avoid resetting the state on GestureScrollEnd generated
        // from the touchpad since it is sent based on a timeout.
        reset_scroll_state = !IsGestureEventFromTouchpad(event);
        break;

      case blink::WebInputEvent::kGestureFlingStart:
        reset_scroll_state = true;
        break;

      case blink::WebInputEvent::kMouseWheel: {
        const blink::WebMouseWheelEvent& wheel =
            static_cast<const blink::WebMouseWheelEvent&>(event);
        if (!wheel.has_precise_scrolling_deltas ||
            wheel.phase == blink::WebMouseWheelEvent::kPhaseEnded ||
            wheel.phase == blink::WebMouseWheelEvent::kPhaseCancelled) {
          reset_scroll_state = true;
        }
        break;
      }

      default:
        if (blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
            blink::WebInputEvent::IsKeyboardEventType(event.GetType())) {
          reset_scroll_state = true;
        }
        break;
    }
  }

  if (reset_scroll_state)
    scroll_state_ = STATE_UNKNOWN;

  if (DispatchEventCompletesAction(event)) {
    CompleteAction();

    // Let the event be dispatched to the renderer.
    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE && DispatchEventResetsState(event)) {
    SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);

    // Let the event be dispatched to the renderer.
    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE) {
    // Consume the event only if it updates the overscroll state.
    if (ProcessEventForOverscroll(event))
      return true;
  } else if (reset_scroll_state) {
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  }


  return false;
}

void OverscrollController::ReceivedEventACK(const blink::WebInputEvent& event,
                                            bool processed) {
  if (!ShouldProcessEvent(event))
    return;

  if (processed) {
    // If a scroll event is consumed by the page, i.e. some content on the page
    // has been scrolled, then there is not going to be an overscroll gesture,
    // until the current scroll ends, and a new scroll gesture starts.
    if (scroll_state_ == STATE_UNKNOWN &&
        (event.GetType() == blink::WebInputEvent::kMouseWheel ||
         event.GetType() == blink::WebInputEvent::kGestureScrollUpdate)) {
      scroll_state_ = STATE_CONTENT_SCROLLING;
    }
    return;
  }
  ProcessEventForOverscroll(event);
}

void OverscrollController::DiscardingGestureEvent(
    const blink::WebGestureEvent& gesture) {
  if (scroll_state_ != STATE_UNKNOWN &&
      (gesture.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
       gesture.GetType() == blink::WebInputEvent::kGestureFlingStart)) {
    scroll_state_ = STATE_UNKNOWN;
  }
}

void OverscrollController::Reset() {
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_source_ = OverscrollSource::NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  scroll_state_ = STATE_UNKNOWN;
}

void OverscrollController::Cancel() {
  SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  scroll_state_ = STATE_UNKNOWN;
}

bool OverscrollController::DispatchEventCompletesAction (
    const blink::WebInputEvent& event) const {
  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;

  // Complete the overscroll gesture if there was a mouse move or a scroll-end
  // after the threshold.
  if (event.GetType() != blink::WebInputEvent::kMouseMove &&
      event.GetType() != blink::WebInputEvent::kGestureScrollEnd &&
      event.GetType() != blink::WebInputEvent::kGestureFlingStart)
    return false;

  // Avoid completing the action on GestureScrollEnd generated
  // from the touchpad since it is sent based on a timeout not
  // when the user has stopped interacting.
  if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd &&
      IsGestureEventFromTouchpad(event)) {
    return false;
  }

  if (!delegate_)
    return false;

  if (event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    // Check to see if the fling is in the same direction of the overscroll.
    const blink::WebGestureEvent gesture =
        static_cast<const blink::WebGestureEvent&>(event);
    switch (overscroll_mode_) {
      case OVERSCROLL_EAST:
        if (gesture.data.fling_start.velocity_x < 0)
          return false;
        break;
      case OVERSCROLL_WEST:
        if (gesture.data.fling_start.velocity_x > 0)
          return false;
        break;
      case OVERSCROLL_NORTH:
        if (gesture.data.fling_start.velocity_y > 0)
          return false;
        break;
      case OVERSCROLL_SOUTH:
        if (gesture.data.fling_start.velocity_y < 0)
          return false;
        break;
      case OVERSCROLL_NONE:
        NOTREACHED();
    }
  }

  const gfx::Size size = delegate_->GetDisplaySize();
  if (size.IsEmpty())
    return false;

  float ratio, threshold;
  if (overscroll_mode_ == OVERSCROLL_WEST ||
      overscroll_mode_ == OVERSCROLL_EAST) {
    ratio = fabs(overscroll_delta_x_) / size.width();
    threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE);
  } else {
    ratio = fabs(overscroll_delta_y_) / size.height();
    threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE);
  }

  return ratio >= threshold;
}

bool OverscrollController::DispatchEventResetsState(
    const blink::WebInputEvent& event) const {
  switch (event.GetType()) {
    case blink::WebInputEvent::kMouseWheel: {
      // Only wheel events with precise deltas (i.e. from trackpad) contribute
      // to the overscroll gesture.
      const blink::WebMouseWheelEvent& wheel =
          static_cast<const blink::WebMouseWheelEvent&>(event);
      return !wheel.has_precise_scrolling_deltas;
    }

    // Avoid resetting overscroll on GestureScrollBegin/End generated
    // from the touchpad since it is sent based on a timeout.
    case blink::WebInputEvent::kGestureScrollBegin:
    case blink::WebInputEvent::kGestureScrollEnd:
      return !IsGestureEventFromTouchpad(event);

    case blink::WebInputEvent::kGestureScrollUpdate:
    case blink::WebInputEvent::kGestureFlingCancel:
      return false;

    default:
      // Touch events can arrive during an overscroll gesture initiated by
      // touch-scrolling. These events should not reset the overscroll state.
      return !blink::WebInputEvent::IsTouchEventType(event.GetType());
  }
}

bool OverscrollController::ProcessEventForOverscroll(
    const blink::WebInputEvent& event) {
  bool event_processed = false;
  switch (event.GetType()) {
    case blink::WebInputEvent::kMouseWheel: {
      const blink::WebMouseWheelEvent& wheel =
          static_cast<const blink::WebMouseWheelEvent&>(event);
      if (!wheel.has_precise_scrolling_deltas)
        break;
      event_processed =
          ProcessOverscroll(wheel.delta_x * wheel.acceleration_ratio_x,
                            wheel.delta_y * wheel.acceleration_ratio_y, true);
      break;
    }
    case blink::WebInputEvent::kGestureScrollUpdate: {
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);
      event_processed = ProcessOverscroll(
          gesture.data.scroll_update.delta_x,
          gesture.data.scroll_update.delta_y,
          gesture.source_device == blink::kWebGestureDeviceTouchpad);
      break;
    }
    case blink::WebInputEvent::kGestureFlingStart: {
      const float kFlingVelocityThreshold = 1100.f;
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);
      float velocity_x = gesture.data.fling_start.velocity_x;
      float velocity_y = gesture.data.fling_start.velocity_y;
      if (fabs(velocity_x) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_WEST && velocity_x < 0) ||
            (overscroll_mode_ == OVERSCROLL_EAST && velocity_x > 0)) {
          CompleteAction();
          event_processed = true;
          break;
        }
      } else if (fabs(velocity_y) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_NORTH && velocity_y < 0) ||
            (overscroll_mode_ == OVERSCROLL_SOUTH && velocity_y > 0)) {
          CompleteAction();
          event_processed = true;
          break;
        }
      }

      // Reset overscroll state if fling didn't complete the overscroll gesture.
      SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
      break;
    }

    default:
      DCHECK(blink::WebInputEvent::IsGestureEventType(event.GetType()) ||
             blink::WebInputEvent::IsTouchEventType(event.GetType()))
          << "Received unexpected event: " << event.GetType();
  }
  return event_processed;
}

bool OverscrollController::ProcessOverscroll(float delta_x,
                                             float delta_y,
                                             bool is_touchpad) {
  if (scroll_state_ != STATE_CONTENT_SCROLLING)
    overscroll_delta_x_ += delta_x;
  overscroll_delta_y_ += delta_y;

  const float horiz_threshold = GetOverscrollConfig(
      is_touchpad ? OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD
                  : OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN);
  const float vert_threshold =
      GetOverscrollConfig(OVERSCROLL_CONFIG_VERT_THRESHOLD_START);
  if (fabs(overscroll_delta_x_) <= horiz_threshold &&
      fabs(overscroll_delta_y_) <= vert_threshold) {
    SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
    return true;
  }

  if (delegate_) {
    base::Optional<float> cap = delegate_->GetMaxOverscrollDelta();
    if (cap) {
      DCHECK_LE(0.f, cap.value());
      switch (overscroll_mode_) {
        case OVERSCROLL_WEST:
        case OVERSCROLL_EAST:
          overscroll_delta_x_ = ClampAbsoluteValue(
              overscroll_delta_x_, cap.value() + horiz_threshold);
          break;
        case OVERSCROLL_NORTH:
        case OVERSCROLL_SOUTH:
          overscroll_delta_y_ = ClampAbsoluteValue(
              overscroll_delta_y_, cap.value() + vert_threshold);
          break;
        case OVERSCROLL_NONE:
          break;
      }
    }
  }

  // Compute the current overscroll direction. If the direction is different
  // from the current direction, then always switch to no-overscroll mode first
  // to make sure that subsequent scroll events go through to the page first.
  OverscrollMode new_mode = OVERSCROLL_NONE;
  const float kMinRatio = 2.5;
  if (fabs(overscroll_delta_x_) > horiz_threshold &&
      fabs(overscroll_delta_x_) > fabs(overscroll_delta_y_) * kMinRatio)
    new_mode = overscroll_delta_x_ > 0.f ? OVERSCROLL_EAST : OVERSCROLL_WEST;
  else if (fabs(overscroll_delta_y_) > vert_threshold &&
           fabs(overscroll_delta_y_) > fabs(overscroll_delta_x_) * kMinRatio)
    new_mode = overscroll_delta_y_ > 0.f ? OVERSCROLL_SOUTH : OVERSCROLL_NORTH;

  // The vertical overscroll is used for pull-to-refresh. Enable it only if
  // pull-to-refresh is enabled.
  if ((new_mode == OVERSCROLL_SOUTH || new_mode == OVERSCROLL_NORTH) &&
      !IsPullToRefreshEnabled())
    new_mode = OVERSCROLL_NONE;

  if (overscroll_mode_ == OVERSCROLL_NONE) {
    SetOverscrollMode(new_mode, is_touchpad ? OverscrollSource::TOUCHPAD
                                            : OverscrollSource::TOUCHSCREEN);
  } else if (new_mode != overscroll_mode_) {
    SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
  }

  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;

  // Tell the delegate about the overscroll update so that it can update
  // the display accordingly (e.g. show history preview etc.).
  if (delegate_) {
    // Do not include the threshold amount when sending the deltas to the
    // delegate.
    float delegate_delta_x = overscroll_delta_x_;
    if (fabs(delegate_delta_x) > horiz_threshold) {
      if (delegate_delta_x < 0)
        delegate_delta_x += horiz_threshold;
      else
        delegate_delta_x -= horiz_threshold;
    } else {
      delegate_delta_x = 0.f;
    }

    float delegate_delta_y = overscroll_delta_y_;
    if (fabs(delegate_delta_y) > vert_threshold) {
      if (delegate_delta_y < 0)
        delegate_delta_y += vert_threshold;
      else
        delegate_delta_y -= vert_threshold;
    } else {
      delegate_delta_y = 0.f;
    }
    return delegate_->OnOverscrollUpdate(delegate_delta_x, delegate_delta_y);
  }
  return false;
}

void OverscrollController::CompleteAction() {
  if (delegate_)
    delegate_->OnOverscrollComplete(overscroll_mode_);
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_source_ = OverscrollSource::NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
}

void OverscrollController::SetOverscrollMode(OverscrollMode mode,
                                             OverscrollSource source) {
  if (overscroll_mode_ == mode)
    return;

  // If the mode changes to NONE, source is also NONE.
  DCHECK(mode != OVERSCROLL_NONE || source == OverscrollSource::NONE);

  OverscrollMode old_mode = overscroll_mode_;
  overscroll_mode_ = mode;
  overscroll_source_ = source;
  if (overscroll_mode_ == OVERSCROLL_NONE)
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  else
    scroll_state_ = STATE_OVERSCROLLING;
  if (delegate_)
    delegate_->OnOverscrollModeChange(old_mode, overscroll_mode_, source);
}

}  // namespace content
