// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_proxy.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/renderer/input/input_handler_proxy_client.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/frame_time.h"

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace {

// Validate provided event timestamps that interact with animation timestamps.
const double kBadTimestampDeltaFromNowInS = 60. * 60. * 24. * 7.;

double InSecondsF(const base::TimeTicks& time) {
  return (time - base::TimeTicks()).InSecondsF();
}

void SendScrollLatencyUma(const WebInputEvent& event,
                          const ui::LatencyInfo& latency_info) {
  if (!(event.type == WebInputEvent::GestureScrollBegin ||
        event.type == WebInputEvent::GestureScrollUpdate ||
        event.type == WebInputEvent::GestureScrollUpdateWithoutPropagation))
    return;

  ui::LatencyInfo::LatencyMap::const_iterator it =
      latency_info.latency_components.find(std::make_pair(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0));

  if (it == latency_info.latency_components.end())
    return;

  base::TimeDelta delta = base::TimeTicks::HighResNow() - it->second.event_time;
  for (size_t i = 0; i < it->second.event_count; ++i) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Event.Latency.RendererImpl.GestureScroll2",
        delta.InMicroseconds(),
        0,
        1000000,
        100);
  }
}  // namespace

}

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
      fling_may_be_active_on_main_thread_(false),
      fling_overscrolled_horizontally_(false),
      fling_overscrolled_vertically_(false) {
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

InputHandlerProxy::EventDisposition
InputHandlerProxy::HandleInputEventWithLatencyInfo(
    const WebInputEvent& event,
    ui::LatencyInfo* latency_info) {
  DCHECK(input_handler_);

  SendScrollLatencyUma(event, *latency_info);

  scoped_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor =
      input_handler_->CreateLatencyInfoSwapPromiseMonitor(latency_info);
  InputHandlerProxy::EventDisposition disposition = HandleInputEvent(event);
  return disposition;
}

InputHandlerProxy::EventDisposition InputHandlerProxy::HandleInputEvent(
    const WebInputEvent& event) {
  DCHECK(client_);
  DCHECK(input_handler_);

  if (event.type == WebInputEvent::MouseWheel) {
    const WebMouseWheelEvent& wheel_event =
        *static_cast<const WebMouseWheelEvent*>(&event);
    if (wheel_event.scrollByPage) {
      // TODO(jamesr): We don't properly handle scroll by page in the compositor
      // thread, so punt it to the main thread. http://crbug.com/236639
      return DID_NOT_HANDLE;
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
        return did_scroll ? DID_HANDLE : DROP_EVENT;
      }
      case cc::InputHandler::ScrollIgnored:
        // TODO(jamesr): This should be DROP_EVENT, but in cases where we fail
        // to properly sync scrollability it's safer to send the event to the
        // main thread. Change back to DROP_EVENT once we have synchronization
        // bugs sorted out.
        return DID_NOT_HANDLE;
      case cc::InputHandler::ScrollOnMainThread:
        return DID_NOT_HANDLE;
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
        return DID_HANDLE;
      case cc::InputHandler::ScrollOnMainThread:
        return DID_NOT_HANDLE;
      case cc::InputHandler::ScrollIgnored:
        return DROP_EVENT;
    }
  } else if (event.type == WebInputEvent::GestureScrollUpdate) {
#ifndef NDEBUG
    DCHECK(expect_scroll_update_end_);
#endif

    if (!gesture_scroll_on_impl_thread_ && !gesture_pinch_on_impl_thread_)
      return DID_NOT_HANDLE;

    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    bool did_scroll = input_handler_->ScrollBy(
        gfx::Point(gesture_event.x, gesture_event.y),
        gfx::Vector2dF(-gesture_event.data.scrollUpdate.deltaX,
                       -gesture_event.data.scrollUpdate.deltaY));
    return did_scroll ? DID_HANDLE : DROP_EVENT;
  } else if (event.type == WebInputEvent::GestureScrollEnd) {
#ifndef NDEBUG
    DCHECK(expect_scroll_update_end_);
    expect_scroll_update_end_ = false;
#endif
    input_handler_->ScrollEnd();

    if (!gesture_scroll_on_impl_thread_)
      return DID_NOT_HANDLE;

    gesture_scroll_on_impl_thread_ = false;
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GesturePinchBegin) {
#ifndef NDEBUG
    DCHECK(!expect_pinch_update_end_);
    expect_pinch_update_end_ = true;
#endif
    input_handler_->PinchGestureBegin();
    gesture_pinch_on_impl_thread_ = true;
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GesturePinchEnd) {
#ifndef NDEBUG
    DCHECK(expect_pinch_update_end_);
    expect_pinch_update_end_ = false;
#endif
    gesture_pinch_on_impl_thread_ = false;
    input_handler_->PinchGestureEnd();
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GesturePinchUpdate) {
#ifndef NDEBUG
    DCHECK(expect_pinch_update_end_);
#endif
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    input_handler_->PinchGestureUpdate(
        gesture_event.data.pinchUpdate.scale,
        gfx::Point(gesture_event.x, gesture_event.y));
    return DID_HANDLE;
  } else if (event.type == WebInputEvent::GestureFlingStart) {
    const WebGestureEvent& gesture_event =
        *static_cast<const WebGestureEvent*>(&event);
    return HandleGestureFling(gesture_event);
  } else if (event.type == WebInputEvent::GestureFlingCancel) {
    if (CancelCurrentFling())
      return DID_HANDLE;
    else if (!fling_may_be_active_on_main_thread_)
      return DROP_EVENT;
  } else if (event.type == WebInputEvent::TouchStart) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    for (size_t i = 0; i < touch_event.touchesLength; ++i) {
      if (touch_event.touches[i].state != WebTouchPoint::StatePressed)
        continue;
      if (input_handler_->HaveTouchEventHandlersAt(touch_event.touches[i]
                                                       .position))
        return DID_NOT_HANDLE;
    }
    return DROP_EVENT;
  } else if (WebInputEvent::isKeyboardEventType(event.type)) {
    CancelCurrentFling();
  }

  return DID_NOT_HANDLE;
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
          blink::WebSize()));
      fling_overscrolled_horizontally_ = false;
      fling_overscrolled_vertically_ = false;
      TRACE_EVENT_ASYNC_BEGIN0(
          "renderer",
          "InputHandlerProxy::HandleGestureFling::started",
          this);
      if (gesture_event.timeStampSeconds) {
        fling_parameters_.startTime = gesture_event.timeStampSeconds;
        DCHECK_LT(fling_parameters_.startTime -
                      InSecondsF(gfx::FrameTime::Now()),
                  kBadTimestampDeltaFromNowInS);
      }
      fling_parameters_.delta =
          WebFloatPoint(gesture_event.data.flingStart.velocityX,
                        gesture_event.data.flingStart.velocityY);
      fling_parameters_.point = WebPoint(gesture_event.x, gesture_event.y);
      fling_parameters_.globalPoint =
          WebPoint(gesture_event.globalX, gesture_event.globalY);
      fling_parameters_.modifiers = gesture_event.modifiers;
      fling_parameters_.sourceDevice = gesture_event.sourceDevice;
      input_handler_->ScheduleAnimation();
      return DID_HANDLE;
    }
    case cc::InputHandler::ScrollOnMainThread: {
      TRACE_EVENT_INSTANT0("renderer",
                           "InputHandlerProxy::HandleGestureFling::"
                           "scroll_on_main_thread",
                           TRACE_EVENT_SCOPE_THREAD);
      fling_may_be_active_on_main_thread_ = true;
      return DID_NOT_HANDLE;
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
        return DID_NOT_HANDLE;
      }
      return DROP_EVENT;
    }
  }
  return DID_NOT_HANDLE;
}

void InputHandlerProxy::Animate(base::TimeTicks time) {
  if (!fling_curve_)
    return;

  double monotonic_time_sec = InSecondsF(time);
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

void InputHandlerProxy::DidOverscroll(const cc::DidOverscrollParams& params) {
  DCHECK(client_);
  if (fling_curve_) {
    static const int kFlingOverscrollThreshold = 1;
    fling_overscrolled_horizontally_ |=
        std::abs(params.accumulated_overscroll.x()) >=
        kFlingOverscrollThreshold;
    fling_overscrolled_vertically_ |=
        std::abs(params.accumulated_overscroll.y()) >=
        kFlingOverscrollThreshold;
  }

  client_->DidOverscroll(params);
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
  fling_parameters_ = blink::WebActiveWheelFlingParameters();
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
      HandleInputEvent(synthetic_wheel);
  switch (disposition) {
    case DID_HANDLE:
      return true;
    case DROP_EVENT:
      break;
    case DID_NOT_HANDLE:
      TRACE_EVENT_INSTANT0("renderer",
                           "InputHandlerProxy::scrollBy::AbortFling",
                           TRACE_EVENT_SCOPE_THREAD);
      // If we got a DID_NOT_HANDLE, that means we need to deliver wheels on the
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
  WebFloatSize clipped_increment;
  if (!fling_overscrolled_horizontally_)
    clipped_increment.width = increment.width;
  if (!fling_overscrolled_vertically_)
    clipped_increment.height = increment.height;

  if (clipped_increment == WebFloatSize())
    return;

  TRACE_EVENT2("renderer",
               "InputHandlerProxy::scrollBy",
               "x",
               clipped_increment.width,
               "y",
               clipped_increment.height);

  bool did_scroll = false;

  switch (fling_parameters_.sourceDevice) {
    case WebGestureEvent::Touchpad:
      did_scroll = TouchpadFlingScroll(clipped_increment);
      break;
    case WebGestureEvent::Touchscreen:
      clipped_increment = ToClientScrollIncrement(clipped_increment);
      did_scroll = input_handler_->ScrollBy(fling_parameters_.point,
                                            clipped_increment);
      break;
  }

  if (did_scroll) {
    fling_parameters_.cumulativeScroll.width += clipped_increment.width;
    fling_parameters_.cumulativeScroll.height += clipped_increment.height;
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
