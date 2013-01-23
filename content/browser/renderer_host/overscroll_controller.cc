// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller.h"

#include "content/browser/renderer_host/gesture_event_filter.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_widget_host_view.h"

namespace content {

OverscrollController::OverscrollController(
    RenderWidgetHostImpl* render_widget_host)
    : render_widget_host_(render_widget_host),
      overscroll_mode_(OVERSCROLL_NONE),
      overscroll_delta_x_(0.f),
      overscroll_delta_y_(0.f),
      delegate_(NULL) {
}

OverscrollController::~OverscrollController() {
}

bool OverscrollController::WillDispatchEvent(
    const WebKit::WebInputEvent& event) {
  if (DispatchEventCompletesAction(event)) {
    CompleteAction();

    // If the overscroll was caused by touch-scrolling, then the gesture event
    // that completes the action needs to be sent to the renderer, because the
    // touch-scrolls maintain state in the renderer side (in the compositor, for
    // example), and the event that completes this action needs to be sent to
    // the renderer so that those states can be updated/reset appropriately.
    // Send the event through the gesture-event filter when appropriate.
    if (ShouldForwardToGestureFilter(event)) {
      const WebKit::WebGestureEvent& gevent =
          static_cast<const WebKit::WebGestureEvent&>(event);
      return render_widget_host_->gesture_event_filter()->
          ShouldForward(gevent);
    }

    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE && DispatchEventResetsState(event)) {
    SetOverscrollMode(OVERSCROLL_NONE);
    // The overscroll gesture status is being reset. If the event is a
    // gesture event (from either touchscreen or trackpad), then make sure the
    // gesture event filter gets the event first (if it didn't already process
    // it).
    if (ShouldForwardToGestureFilter(event)) {
      const WebKit::WebGestureEvent& gevent =
          static_cast<const WebKit::WebGestureEvent&>(event);
      return render_widget_host_->gesture_event_filter()->ShouldForward(gevent);
    }

    // Let the event be dispatched to the renderer.
    return true;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE) {
    // Consume the event and update overscroll state when in the middle of the
    // overscroll gesture.
    ProcessEventForOverscroll(event);
    return false;
  }

  return true;
}

void OverscrollController::ReceivedEventACK(const WebKit::WebInputEvent& event,
                                            bool processed) {
  if (processed)
    return;
  ProcessEventForOverscroll(event);
}

void OverscrollController::Reset() {
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
}

bool OverscrollController::DispatchEventCompletesAction (
    const WebKit::WebInputEvent& event) const {
  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;

  // Complete the overscroll gesture if there was a mouse move or a scroll-end
  // after the threshold.
  if (event.type != WebKit::WebInputEvent::MouseMove &&
      event.type != WebKit::WebInputEvent::GestureScrollEnd &&
      event.type != WebKit::WebInputEvent::GestureFlingStart)
    return false;

  RenderWidgetHostView* view = render_widget_host_->GetView();
  if (!view->IsShowing())
    return false;

  const gfx::Rect& bounds = view->GetViewBounds();
  if (bounds.IsEmpty())
    return false;

  if (event.type == WebKit::WebInputEvent::GestureFlingStart) {
    // Check to see if the fling is in the same direction of the overscroll.
    const WebKit::WebGestureEvent gesture =
        static_cast<const WebKit::WebGestureEvent&>(event);
    switch (overscroll_mode_) {
      case OVERSCROLL_EAST:
        if (gesture.data.flingStart.velocityX < 0)
          return false;
        break;
      case OVERSCROLL_WEST:
        if (gesture.data.flingStart.velocityX > 0)
          return false;
        break;
      case OVERSCROLL_NORTH:
        if (gesture.data.flingStart.velocityY > 0)
          return false;
        break;
      case OVERSCROLL_SOUTH:
        if (gesture.data.flingStart.velocityY < 0)
          return false;
        break;
      case OVERSCROLL_NONE:
        NOTREACHED();
    }
  }

  float ratio, threshold;
  if (overscroll_mode_ == OVERSCROLL_WEST ||
      overscroll_mode_ == OVERSCROLL_EAST) {
    ratio = fabs(overscroll_delta_x_) / bounds.width();
    threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE);
  } else {
    ratio = fabs(overscroll_delta_y_) / bounds.height();
    threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE);
  }

  return ratio >= threshold;
}

bool OverscrollController::DispatchEventResetsState(
    const WebKit::WebInputEvent& event) const {
  switch (event.type) {
    case WebKit::WebInputEvent::MouseWheel:
    case WebKit::WebInputEvent::GestureScrollUpdate:
    case WebKit::WebInputEvent::GestureFlingCancel:
      return false;

    default:
      // Touch events can arrive during an overscroll gesture initiated by
      // touch-scrolling. These events should not reset the overscroll state.
      return !WebKit::WebInputEvent::isTouchEventType(event.type);
  }
}

void OverscrollController::ProcessEventForOverscroll(
    const WebKit::WebInputEvent& event) {
  switch (event.type) {
    case WebKit::WebInputEvent::MouseWheel: {
      const WebKit::WebMouseWheelEvent& wheel =
          static_cast<const WebKit::WebMouseWheelEvent&>(event);
      ProcessOverscroll(wheel.deltaX, wheel.deltaY);
      break;
    }
    case WebKit::WebInputEvent::GestureScrollUpdate: {
      const WebKit::WebGestureEvent& gesture =
          static_cast<const WebKit::WebGestureEvent&>(event);
      ProcessOverscroll(gesture.data.scrollUpdate.deltaX,
                        gesture.data.scrollUpdate.deltaY);
      break;
    }
    case WebKit::WebInputEvent::GestureFlingStart: {
      const float kFlingVelocityThreshold = 1100.f;
      const WebKit::WebGestureEvent& gesture =
          static_cast<const WebKit::WebGestureEvent&>(event);
      float velocity_x = gesture.data.flingStart.velocityX;
      float velocity_y = gesture.data.flingStart.velocityY;
      if (fabs(velocity_x) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_WEST && velocity_x < 0) ||
            (overscroll_mode_ == OVERSCROLL_EAST && velocity_x > 0)) {
          CompleteAction();
          break;
        }
      } else if (fabs(velocity_y) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_NORTH && velocity_y < 0) ||
            (overscroll_mode_ == OVERSCROLL_SOUTH && velocity_y > 0)) {
          CompleteAction();
          break;
        }
      }

      // Reset overscroll state if fling didn't complete the overscroll gesture.
      SetOverscrollMode(OVERSCROLL_NONE);
      break;
    }

    default:
      DCHECK(WebKit::WebInputEvent::isGestureEventType(event.type) ||
             WebKit::WebInputEvent::isTouchEventType(event.type))
          << "Received unexpected event: " << event.type;
  }
}

void OverscrollController::ProcessOverscroll(float delta_x, float delta_y) {
  overscroll_delta_x_ += delta_x;
  overscroll_delta_y_ += delta_y;

  float threshold = GetOverscrollConfig(OVERSCROLL_CONFIG_MIN_THRESHOLD_START);
  if (fabs(overscroll_delta_x_) < threshold &&
      fabs(overscroll_delta_y_) < threshold) {
    SetOverscrollMode(OVERSCROLL_NONE);
    return;
  }

  // The scroll may have changed the overscroll mode. Set the mode correctly
  // first before informing the delegate about the update in overscroll amount.
  if (fabs(overscroll_delta_x_) > fabs(overscroll_delta_y_)) {
    SetOverscrollMode(overscroll_delta_x_ > 0.f ? OVERSCROLL_EAST :
                                                  OVERSCROLL_WEST);
  } else {
    SetOverscrollMode(overscroll_delta_y_ > 0.f ? OVERSCROLL_SOUTH :
                                                  OVERSCROLL_NORTH);
  }

  // Tell the delegate about the overscroll update so that it can update
  // the display accordingly (e.g. show history preview etc.).
  if (delegate_) {
    // Do not include the threshold amount when sending the deltas to the
    // delegate.
    float delegate_delta_x = overscroll_delta_x_;
    if (fabs(delegate_delta_x) > threshold) {
      if (delegate_delta_x < 0)
        delegate_delta_x += threshold;
      else
        delegate_delta_x -= threshold;
    } else {
      delegate_delta_x = 0.f;
    }

    float delegate_delta_y = overscroll_delta_y_;
    if (fabs(delegate_delta_y) > threshold) {
      if (delegate_delta_y < 0)
        delegate_delta_y += threshold;
      else
        delegate_delta_y -= threshold;
    } else {
      delegate_delta_y = 0.f;
    }
    delegate_->OnOverscrollUpdate(delegate_delta_x, delegate_delta_y);
  }
}

void OverscrollController::CompleteAction() {
  if (delegate_)
    delegate_->OnOverscrollComplete(overscroll_mode_);
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
}

void OverscrollController::SetOverscrollMode(OverscrollMode mode) {
  if (overscroll_mode_ == mode)
    return;
  OverscrollMode old_mode = overscroll_mode_;
  overscroll_mode_ = mode;
  if (overscroll_mode_ == OVERSCROLL_NONE)
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  if (delegate_)
    delegate_->OnOverscrollModeChange(old_mode, overscroll_mode_);
}

bool OverscrollController::ShouldForwardToGestureFilter(
    const WebKit::WebInputEvent& event) const {
  if (!WebKit::WebInputEvent::isGestureEventType(event.type))
    return false;

  // If the GestureEventFilter already processed this event, then the event must
  // not be sent to the filter again.
  return !render_widget_host_->gesture_event_filter()->HasQueuedGestureEvents();
}

}  // namespace content
