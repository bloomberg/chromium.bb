// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/input_handler_proxy.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/renderer/gpu/input_handler_proxy_client.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebFloatPoint;
using WebKit::WebFloatSize;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebPoint;
using WebKit::WebTouchEvent;

namespace content {

InputHandlerProxy::InputHandlerProxy(cc::InputHandler* input_handler)
    : client_(NULL),
      input_handler_(input_handler),
#ifndef NDEBUG
      expect_scroll_update_end_(false),
      expect_pinch_update_end_(false),
#endif
      gesture_scroll_on_impl_thread_(false),
      gesture_pinch_on_impl_thread_(false),
      fling_may_be_active_on_main_thread_(false) {
  input_handler_->BindToClient(this);
}

InputHandlerProxy::~InputHandlerProxy() {}

void InputHandlerProxy::WillShutdown() {
  input_handler_ = NULL;
  DCHECK(client_);
  client_->WillShutdown();
}

void InputHandlerProxy::SetClient(InputHandlerProxyClient* client) {
  DCHECK(!client_ || !client);
  client_ = client;
}

void InputHandlerProxy::HandleInputEvent(const WebInputEvent& event) {
  DCHECK(client_);
  DCHECK(input_handler_);

  InputHandlerProxy::EventDisposition disposition =
      HandleInputEventInternal(event);
  switch (disposition) {
    case DidHandle:
      client_->DidHandleInputEvent();
      break;
    case DidNotHandle:
      client_->DidNotHandleInputEvent(true /* send_to_widget */);
      break;
    case DropEvent:
      client_->DidNotHandleInputEvent(false /* send_to_widget */);
      break;
  }
  if (event.modifiers & WebInputEvent::IsLastInputEventForCurrentVSync) {
    input_handler_->DidReceiveLastInputEventForBeginFrame(
        base::TimeTicks::FromInternalValue(event.timeStampSeconds *
                                           base::Time::kMicrosecondsPerSecond));
  }
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleInputEventInternal(const WebInputEvent& event) {
  if (event.type == WebInputEvent::MouseWheel) {
    const WebMouseWheelEvent& wheel_event =
        *static_cast<const WebMouseWheelEvent*>(&event);
    if (wheel_event.scrollByPage) {
      // TODO(jamesr): We don't properly handle scroll by page in the compositor
      // thread, so punt it to the main thread. http://crbug.com/236639
      return DidNotHandle;
    }
    cc::InputHandler::ScrollStatus scroll_status = input_handler_->ScrollBegin(
        gfx::Point(wheel_event.x, wheel_event.y), cc::InputHandler::Wheel);
    switch (scroll_status) {
      case cc::InputHandler::ScrollStarted: {
        TRACE_EVENT_INSTANT2(
            "renderer",
            "InputHandlerProxy::handle_input wheel scroll",
            TRACE_EVENT_SCOPE_THREAD,
            "deltaX",
            -wheel_event.deltaX,
            "deltaY",
            -wheel_event.deltaY);
        bool did_scroll = input_handler_->ScrollBy(
            gfx::Point(wheel_event.x, wheel_event.y),
            gfx::Vector2dF(-wheel_event.deltaX, -wheel_event.deltaY));
        input_handler_->ScrollEnd();
        return did_scroll ? DidHandle : DropEvent;
      }
      case cc::InputHandler::ScrollIgnored:
        // TODO(jamesr): This should be DropEvent, but in cases where we fail to
        // properly sync scrollability it's safer to send the
        // event to the main thread. Change back to DropEvent once we have
        // synchronization bugs sorted out.
        return DidNotHandle;
      case cc::InputHandler::ScrollOnMainThread:
        return DidNotHandle;
    }
  } else if (event.type == WebInputEvent::GestureScrollBegin) {
    DCHECK(!gesture_scroll_on_impl_thread_);
#ifndef NDEBUG
    DCHECK(!expect_scroll_update_end_);
    expect_scroll_update_end_ = true;
#endif
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    cc::InputHandler::ScrollStatus scroll_status = input_handler_->ScrollBegin(
        gfx::Point(gesture_event.x, gesture_event.y),
        cc::InputHandler::Gesture);
    switch (scroll_status) {
      case cc::InputHandler::ScrollStarted:
        gesture_scroll_on_impl_thread_ = true;
        return DidHandle;
      case cc::InputHandler::ScrollOnMainThread:
        return DidNotHandle;
      case cc::InputHandler::ScrollIgnored:
        return DropEvent;
    }
  } else if (event.type == WebInputEvent::GestureScrollUpdate) {
#ifndef NDEBUG
    DCHECK(expect_scroll_update_end_);
#endif

    if (!gesture_scroll_on_impl_thread_ && !gesture_pinch_on_impl_thread_)
      return DidNotHandle;

    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    bool did_scroll = input_handler_->ScrollBy(
        gfx::Point(gesture_event.x, gesture_event.y),
        gfx::Vector2dF(-gesture_event.data.scrollUpdate.deltaX,
                       -gesture_event.data.scrollUpdate.deltaY));
    return did_scroll ? DidHandle : DropEvent;
  } else if (event.type == WebInputEvent::GestureScrollEnd) {
#ifndef NDEBUG
    DCHECK(expect_scroll_update_end_);
    expect_scroll_update_end_ = false;
#endif
    if (!gesture_scroll_on_impl_thread_)
      return DidNotHandle;

    input_handler_->ScrollEnd();
    gesture_scroll_on_impl_thread_ = false;
    return DidHandle;
  } else if (event.type == WebInputEvent::GesturePinchBegin) {
#ifndef NDEBUG
    DCHECK(!expect_pinch_update_end_);
    expect_pinch_update_end_ = true;
#endif
    input_handler_->PinchGestureBegin();
    gesture_pinch_on_impl_thread_ = true;
    return DidHandle;
  } else if (event.type == WebInputEvent::GesturePinchEnd) {
#ifndef NDEBUG
    DCHECK(expect_pinch_update_end_);
    expect_pinch_update_end_ = false;
#endif
    gesture_pinch_on_impl_thread_ = false;
    input_handler_->PinchGestureEnd();
    return DidHandle;
  } else if (event.type == WebInputEvent::GesturePinchUpdate) {
#ifndef NDEBUG
    DCHECK(expect_pinch_update_end_);
#endif
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    input_handler_->PinchGestureUpdate(
        gesture_event.data.pinchUpdate.scale,
        gfx::Point(gesture_event.x, gesture_event.y));
    return DidHandle;
  } else if (event.type == WebInputEvent::GestureFlingStart) {
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    return HandleGestureFling(gesture_event);
  } else if (event.type == WebInputEvent::GestureFlingCancel) {
    if (CancelCurrentFling())
      return DidHandle;
    else if (!fling_may_be_active_on_main_thread_)
      return DropEvent;
  } else if (event.type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    if (!input_handler_->HaveTouchEventHandlersAt(touch_event.touches[0]
                                                      .position))
      return DropEvent;
  } else if (WebInputEvent::isKeyboardEventType(event.type)) {
    CancelCurrentFling();
  }

  return DidNotHandle;
}

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleGestureFling(
    const WebGestureEvent& gesture_event) {
  cc::InputHandler::ScrollStatus scroll_status;

  if (gesture_event.sourceDevice == WebGestureEvent::Touchpad) {
    scroll_status = input_handler_->ScrollBegin(
        gfx::Point(gesture_event.x, gesture_event.y),
        cc::InputHandler::NonBubblingGesture);
  } else {
    if (!gesture_scroll_on_impl_thread_)
      scroll_status = cc::InputHandler::ScrollOnMainThread;
    else
      scroll_status = input_handler_->FlingScrollBegin();
  }

#ifndef NDEBUG
  expect_scroll_update_end_ = false;
#endif

  switch (scroll_status) {
    case cc::InputHandler::ScrollStarted: {
      if (gesture_event.sourceDevice == WebGestureEvent::Touchpad)
        input_handler_->ScrollEnd();

      fling_curve_.reset(client_->CreateFlingAnimationCurve(
          gesture_event.sourceDevice,
          WebFloatPoint(gesture_event.data.flingStart.velocityX,
                        gesture_event.data.flingStart.velocityY),
          WebKit::WebSize()));
      TRACE_EVENT_ASYNC_BEGIN0(
          "renderer",
          "InputHandlerProxy::HandleGestureFling::started",
          this);
      fling_parameters_.delta =
          WebFloatPoint(gesture_event.data.flingStart.velocityX,
                        gesture_event.data.flingStart.velocityY);
      fling_parameters_.point = WebPoint(gesture_event.x, gesture_event.y);
      fling_parameters_.globalPoint =
          WebPoint(gesture_event.globalX, gesture_event.globalY);
      fling_parameters_.modifiers = gesture_event.modifiers;
      fling_parameters_.sourceDevice = gesture_event.sourceDevice;
      input_handler_->ScheduleAnimation();
      return DidHandle;
    }
    case cc::InputHandler::ScrollOnMainThread: {
      TRACE_EVENT_INSTANT0("renderer",
                           "InputHandlerProxy::HandleGestureFling::"
                           "scroll_on_main_thread",
                           TRACE_EVENT_SCOPE_THREAD);
      fling_may_be_active_on_main_thread_ = true;
      return DidNotHandle;
    }
    case cc::InputHandler::ScrollIgnored: {
      TRACE_EVENT_INSTANT0(
          "renderer",
          "InputHandlerProxy::HandleGestureFling::ignored",
          TRACE_EVENT_SCOPE_THREAD);
      if (gesture_event.sourceDevice == WebGestureEvent::Touchpad) {
        // We still pass the curve to the main thread if there's nothing
        // scrollable, in case something
        // registers a handler before the curve is over.
        return DidNotHandle;
      }
      return DropEvent;
    }
  }
  return DidNotHandle;
}

void InputHandlerProxy::Animate(base::TimeTicks time) {
  if (!fling_curve_)
    return;

  double monotonic_time_sec = (time - base::TimeTicks()).InSecondsF();
  if (!fling_parameters_.startTime) {
    fling_parameters_.startTime = monotonic_time_sec;
    input_handler_->ScheduleAnimation();
    return;
  }

  if (fling_curve_->apply(monotonic_time_sec - fling_parameters_.startTime,
                          this)) {
    input_handler_->ScheduleAnimation();
  } else {
    TRACE_EVENT_INSTANT0("renderer",
                         "InputHandlerProxy::animate::flingOver",
                         TRACE_EVENT_SCOPE_THREAD);
    CancelCurrentFling();
  }
}

void InputHandlerProxy::MainThreadHasStoppedFlinging() {
  fling_may_be_active_on_main_thread_ = false;
}

void InputHandlerProxy::DidOverscroll(gfx::Vector2dF accumulated_overscroll,
                                      gfx::Vector2dF current_fling_velocity) {
  DCHECK(client_);
  client_->DidOverscroll(accumulated_overscroll, current_fling_velocity);
}

bool InputHandlerProxy::CancelCurrentFling() {
  bool had_fling_animation = fling_curve_;
  if (had_fling_animation &&
      fling_parameters_.sourceDevice == WebGestureEvent::Touchscreen) {
    input_handler_->ScrollEnd();
    TRACE_EVENT_ASYNC_END0(
        "renderer",
        "InputHandlerProxy::HandleGestureFling::started",
        this);
  }

  TRACE_EVENT_INSTANT1("renderer",
                       "InputHandlerProxy::CancelCurrentFling",
                       TRACE_EVENT_SCOPE_THREAD,
                       "had_fling_animation",
                       had_fling_animation);
  fling_curve_.reset();
  gesture_scroll_on_impl_thread_ = false;
  fling_parameters_ = WebKit::WebActiveWheelFlingParameters();
  return had_fling_animation;
}

bool InputHandlerProxy::TouchpadFlingScroll(
    const WebFloatSize& increment) {
  WebMouseWheelEvent synthetic_wheel;
  synthetic_wheel.type = WebInputEvent::MouseWheel;
  synthetic_wheel.deltaX = increment.width;
  synthetic_wheel.deltaY = increment.height;
  synthetic_wheel.hasPreciseScrollingDeltas = true;
  synthetic_wheel.x = fling_parameters_.point.x;
  synthetic_wheel.y = fling_parameters_.point.y;
  synthetic_wheel.globalX = fling_parameters_.globalPoint.x;
  synthetic_wheel.globalY = fling_parameters_.globalPoint.y;
  synthetic_wheel.modifiers = fling_parameters_.modifiers;

  InputHandlerProxy::EventDisposition disposition =
      HandleInputEventInternal(synthetic_wheel);
  switch (disposition) {
    case DidHandle:
      return true;
    case DropEvent:
      break;
    case DidNotHandle:
      TRACE_EVENT_INSTANT0("renderer",
                           "InputHandlerProxy::scrollBy::AbortFling",
                           TRACE_EVENT_SCOPE_THREAD);
      // If we got a DidNotHandle, that means we need to deliver wheels on the
      // main thread. In this case we need to schedule a commit and transfer the
      // fling curve over to the main thread and run the rest of the wheels from
      // there. This can happen when flinging a page that contains a scrollable
      // subarea that we can't scroll on the thread if the fling starts outside
      // the subarea but then is flung "under" the pointer.
      client_->TransferActiveWheelFlingAnimation(fling_parameters_);
      fling_may_be_active_on_main_thread_ = true;
      CancelCurrentFling();
      break;
  }

  return false;
}

static gfx::Vector2dF ToClientScrollIncrement(const WebFloatSize& increment) {
  return gfx::Vector2dF(-increment.width, -increment.height);
}

void InputHandlerProxy::scrollBy(const WebFloatSize& increment) {
  if (increment == WebFloatSize())
    return;

  TRACE_EVENT2("renderer",
               "InputHandlerProxy::scrollBy",
               "x",
               increment.width,
               "y",
               increment.height);

  bool did_scroll = false;

  switch (fling_parameters_.sourceDevice) {
    case WebGestureEvent::Touchpad:
      did_scroll = TouchpadFlingScroll(increment);
      break;
    case WebGestureEvent::Touchscreen:
      did_scroll = input_handler_->ScrollBy(fling_parameters_.point,
                                            ToClientScrollIncrement(increment));
      break;
  }

  if (did_scroll) {
    fling_parameters_.cumulativeScroll.width += increment.width;
    fling_parameters_.cumulativeScroll.height += increment.height;
  }
}

void InputHandlerProxy::notifyCurrentFlingVelocity(
    const WebFloatSize& velocity) {
  TRACE_EVENT2("renderer",
               "InputHandlerProxy::notifyCurrentFlingVelocity",
               "vx",
               velocity.width,
               "vy",
               velocity.height);
  input_handler_->NotifyCurrentFlingVelocity(ToClientScrollIncrement(velocity));
}

}  // namespace content
