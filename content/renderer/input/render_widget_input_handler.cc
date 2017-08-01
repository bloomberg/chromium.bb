// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/render_widget_input_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "cc/trees/swap_promise_monitor.h"
#include "content/common/input/input_event_ack.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebKeyboardEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "third_party/WebKit/public/platform/WebTouchEvent.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/latency/latency_info.h"

#if defined(OS_ANDROID)
#include <android/keycodes.h>
#endif

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::DidOverscrollParams;

namespace content {

namespace {

int64_t GetEventLatencyMicros(double event_timestamp, base::TimeTicks now) {
  return (now - base::TimeDelta::FromSecondsD(event_timestamp))
      .ToInternalValue();
}

void LogInputEventLatencyUma(const WebInputEvent& event, base::TimeTicks now) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Event.AggregatedLatency.Renderer2",
      GetEventLatencyMicros(event.TimeStampSeconds(), now), 1, 10000000, 100);
}

void LogPassiveEventListenersUma(WebInputEventResult result,
                                 WebInputEvent::DispatchType dispatch_type,
                                 double event_timestamp,
                                 const ui::LatencyInfo& latency_info) {
  enum ListenerEnum {
    PASSIVE_LISTENER_UMA_ENUM_PASSIVE,
    PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
    PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED,
    PASSIVE_LISTENER_UMA_ENUM_CANCELABLE,
    PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED,
    PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING,
    PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
    PASSIVE_LISTENER_UMA_ENUM_COUNT
  };

  ListenerEnum enum_value;
  switch (dispatch_type) {
    case WebInputEvent::kListenersForcedNonBlockingDueToFling:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING;
      break;
    case WebInputEvent::
        kListenersForcedNonBlockingDueToMainThreadResponsiveness:
      enum_value =
          PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS;
      break;
    case WebInputEvent::kListenersNonBlockingPassive:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_PASSIVE;
      break;
    case WebInputEvent::kEventNonBlocking:
      enum_value = PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE;
      break;
    case WebInputEvent::kBlocking:
      if (result == WebInputEventResult::kHandledApplication)
        enum_value = PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED;
      else if (result == WebInputEventResult::kHandledSuppressed)
        enum_value = PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED;
      else
        enum_value = PASSIVE_LISTENER_UMA_ENUM_CANCELABLE;
      break;
    default:
      NOTREACHED();
      return;
  }

  UMA_HISTOGRAM_ENUMERATION("Event.PassiveListeners", enum_value,
                            PASSIVE_LISTENER_UMA_ENUM_COUNT);

  if (base::TimeTicks::IsHighResolution()) {
    if (enum_value == PASSIVE_LISTENER_UMA_ENUM_CANCELABLE) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.PassiveListeners.Latency",
                                  GetEventLatencyMicros(event_timestamp, now),
                                  1, 10000000, 100);
    } else if (enum_value ==
               PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.PassiveListeners.ForcedNonBlockingLatencyDueToFling",
          GetEventLatencyMicros(event_timestamp, now), 1, 10000000, 100);
    } else if (
        enum_value ==
        PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.PassiveListeners."
          "ForcedNonBlockingLatencyDueToUnresponsiveMainThread",
          GetEventLatencyMicros(event_timestamp, now), 1, 10000000, 50);
    }
  }
}

}  // namespace

RenderWidgetInputHandler::RenderWidgetInputHandler(
    RenderWidgetInputHandlerDelegate* delegate,
    RenderWidget* widget)
    : delegate_(delegate),
      widget_(widget),
      handling_input_event_(false),
      handling_event_overscroll_(nullptr),
      handling_event_type_(WebInputEvent::kUndefined),
      suppress_next_char_events_(false) {
  DCHECK(delegate);
  DCHECK(widget);
  delegate->SetInputHandler(this);
}

RenderWidgetInputHandler::~RenderWidgetInputHandler() {}

void RenderWidgetInputHandler::HandleInputEvent(
    const blink::WebCoalescedInputEvent& coalesced_event,
    const ui::LatencyInfo& latency_info,
    HandledEventCallback callback) {
  const WebInputEvent& input_event = coalesced_event.Event();
  base::AutoReset<bool> handling_input_event_resetter(&handling_input_event_,
                                                      true);
  base::AutoReset<WebInputEvent::Type> handling_event_type_resetter(
      &handling_event_type_, input_event.GetType());

  // Calls into |didOverscroll()| while handling this event will populate
  // |event_overscroll|, which in turn will be bundled with the event ack.
  std::unique_ptr<DidOverscrollParams> event_overscroll;
  base::AutoReset<std::unique_ptr<DidOverscrollParams>*>
      handling_event_overscroll_resetter(&handling_event_overscroll_,
                                         &event_overscroll);

#if defined(OS_ANDROID)
  ImeEventGuard guard(widget_);
#endif

  base::TimeTicks start_time;
  if (base::TimeTicks::IsHighResolution())
    start_time = base::TimeTicks::Now();

  TRACE_EVENT1("renderer,benchmark,rail",
               "RenderWidgetInputHandler::OnHandleInputEvent", "event",
               WebInputEvent::GetName(input_event.GetType()));
  TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "HandleInputEventMain");

  // If we don't have a high res timer, these metrics won't be accurate enough
  // to be worth collecting. Note that this does introduce some sampling bias.
  if (!start_time.is_null())
    LogInputEventLatencyUma(input_event, start_time);

  std::unique_ptr<cc::SwapPromiseMonitor> latency_info_swap_promise_monitor;
  ui::LatencyInfo swap_latency_info(latency_info);

  if (RenderThreadImpl::current()) {
    swap_latency_info.set_expected_queueing_time_on_dispatch(
        RenderThreadImpl::current()
            ->GetRendererScheduler()
            ->MostRecentExpectedQueueingTime());
  }

  swap_latency_info.AddLatencyNumber(
      ui::LatencyComponentType::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0,
      0);
  if (widget_->compositor()) {
    latency_info_swap_promise_monitor =
        widget_->compositor()->CreateLatencyInfoSwapPromiseMonitor(
            &swap_latency_info);
  }

  bool prevent_default = false;
  if (WebInputEvent::IsMouseEventType(input_event.GetType())) {
    const WebMouseEvent& mouse_event =
        static_cast<const WebMouseEvent&>(input_event);
    TRACE_EVENT2("renderer", "HandleMouseMove", "x",
                 mouse_event.PositionInWidget().x, "y",
                 mouse_event.PositionInWidget().y);
    prevent_default = delegate_->WillHandleMouseEvent(mouse_event);
  }

  if (WebInputEvent::IsKeyboardEventType(input_event.GetType())) {
#if defined(OS_ANDROID)
    // The DPAD_CENTER key on Android has a dual semantic: (1) in the general
    // case it should behave like a select key (i.e. causing a click if a button
    // is focused). However, if a text field is focused (2), its intended
    // behavior is to just show the IME and don't propagate the key.
    // A typical use case is a web form: the DPAD_CENTER should bring up the IME
    // when clicked on an input text field and cause the form submit if clicked
    // when the submit button is focused, but not vice-versa.
    // The UI layer takes care of translating DPAD_CENTER into a RETURN key,
    // but at this point we have to swallow the event for the scenario (2).
    const WebKeyboardEvent& key_event =
        static_cast<const WebKeyboardEvent&>(input_event);
    if (key_event.native_key_code == AKEYCODE_DPAD_CENTER &&
        widget_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      widget_->ShowVirtualKeyboardOnElementFocus();
      prevent_default = true;
    }
#endif
  }

  if (WebInputEvent::IsGestureEventType(input_event.GetType())) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    prevent_default =
        prevent_default || delegate_->WillHandleGestureEvent(gesture_event);
  }

  WebInputEventResult processed = prevent_default
                                      ? WebInputEventResult::kHandledSuppressed
                                      : WebInputEventResult::kNotHandled;
  if (input_event.GetType() != WebInputEvent::kChar ||
      !suppress_next_char_events_) {
    suppress_next_char_events_ = false;
    if (processed == WebInputEventResult::kNotHandled &&
        widget_->GetWebWidget())
      processed = widget_->GetWebWidget()->HandleInputEvent(coalesced_event);
  }

  // TODO(dtapuska): Use the input_event.timeStampSeconds as the start
  // ideally this should be when the event was sent by the compositor to the
  // renderer. crbug.com/565348
  if (input_event.GetType() == WebInputEvent::kTouchStart ||
      input_event.GetType() == WebInputEvent::kTouchMove ||
      input_event.GetType() == WebInputEvent::kTouchEnd) {
    const WebTouchEvent& touch = static_cast<const WebTouchEvent&>(input_event);

    LogPassiveEventListenersUma(processed, touch.dispatch_type,
                                input_event.TimeStampSeconds(), latency_info);

    // TODO(lanwei): Remove this metric for event latency outside fling in M56,
    // once we've gathered enough data to decide if we want to ship the passive
    // event listener for fling, see https://crbug.com/638661.
    if (touch.dispatch_type == WebInputEvent::kBlocking &&
        touch.touch_start_or_first_touch_move &&
        base::TimeTicks::IsHighResolution()) {
      base::TimeTicks now = base::TimeTicks::Now();
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Event.Touch.TouchLatencyOutsideFling",
          GetEventLatencyMicros(input_event.TimeStampSeconds(), now), 1,
          100000000, 50);
    }
  } else if (input_event.GetType() == WebInputEvent::kMouseWheel) {
    LogPassiveEventListenersUma(
        processed,
        static_cast<const WebMouseWheelEvent&>(input_event).dispatch_type,
        input_event.TimeStampSeconds(), latency_info);
  }

  // If this RawKeyDown event corresponds to a browser keyboard shortcut and
  // it's not processed by webkit, then we need to suppress the upcoming Char
  // events.
  bool is_keyboard_shortcut =
      input_event.GetType() == WebInputEvent::kRawKeyDown &&
      static_cast<const WebKeyboardEvent&>(input_event).is_browser_shortcut;
  if (processed == WebInputEventResult::kNotHandled && is_keyboard_shortcut)
    suppress_next_char_events_ = true;

  InputEventAckState ack_result = processed == WebInputEventResult::kNotHandled
                                      ? INPUT_EVENT_ACK_STATE_NOT_CONSUMED
                                      : INPUT_EVENT_ACK_STATE_CONSUMED;
  if (processed == WebInputEventResult::kNotHandled &&
      input_event.GetType() == WebInputEvent::kTouchStart) {
    const WebTouchEvent& touch_event =
        static_cast<const WebTouchEvent&>(input_event);
    // Hit-test for all the pressed touch points. If there is a touch-handler
    // for any of the touch points, then the renderer should continue to receive
    // touch events.
    ack_result = INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    for (size_t i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].state == WebTouchPoint::kStatePressed &&
          delegate_->HasTouchEventHandlersAt(
              gfx::ToFlooredPoint(touch_event.touches[i].PositionInWidget()))) {
        ack_result = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
        break;
      }
    }
  }

  // Send gesture scroll events and their dispositions to the compositor thread,
  // so that they can be used to produce the elastic overscroll effect on Mac.
  if (input_event.GetType() == WebInputEvent::kGestureScrollBegin ||
      input_event.GetType() == WebInputEvent::kGestureScrollEnd ||
      input_event.GetType() == WebInputEvent::kGestureScrollUpdate) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(input_event);
    if (gesture_event.source_device == blink::kWebGestureDeviceTouchpad) {
      delegate_->ObserveGestureEventAndResult(
          gesture_event,
          event_overscroll ? event_overscroll->latest_overscroll_delta
                           : gfx::Vector2dF(),
          processed != WebInputEventResult::kNotHandled);
    }
  }

  if (callback) {
    std::move(callback).Run(ack_result, swap_latency_info,
                            std::move(event_overscroll));
  } else {
    DCHECK(!event_overscroll) << "Unexpected overscroll for un-acked event";
  }

  // Show the virtual keyboard if enabled and a user gesture triggers a focus
  // change.
  if (processed != WebInputEventResult::kNotHandled &&
      (input_event.GetType() == WebInputEvent::kTouchEnd ||
       input_event.GetType() == WebInputEvent::kMouseUp)) {
    delegate_->ShowVirtualKeyboard();
  }

  if (!prevent_default &&
      WebInputEvent::IsKeyboardEventType(input_event.GetType()))
    delegate_->OnDidHandleKeyEvent();

// TODO(rouslan): Fix ChromeOS and Windows 8 behavior of autofill popup with
// virtual keyboard.
#if !defined(OS_ANDROID)
  // Virtual keyboard is not supported, so react to focus change immediately.
  if (processed != WebInputEventResult::kNotHandled &&
      (input_event.GetType() == WebInputEvent::kTouchEnd ||
       input_event.GetType() == WebInputEvent::kMouseUp)) {
    delegate_->FocusChangeComplete();
  }
#endif
}

void RenderWidgetInputHandler::DidOverscrollFromBlink(
    const WebFloatSize& overscrollDelta,
    const WebFloatSize& accumulatedOverscroll,
    const WebFloatPoint& position,
    const WebFloatSize& velocity) {
  std::unique_ptr<DidOverscrollParams> params(new DidOverscrollParams());
  params->accumulated_overscroll = gfx::Vector2dF(
      accumulatedOverscroll.width, accumulatedOverscroll.height);
  params->latest_overscroll_delta =
      gfx::Vector2dF(overscrollDelta.width, overscrollDelta.height);
  params->current_fling_velocity =
      gfx::Vector2dF(velocity.width, velocity.height);
  params->causal_event_viewport_point = gfx::PointF(position.x, position.y);

  // If we're currently handling an event, stash the overscroll data such that
  // it can be bundled in the event ack.
  if (handling_event_overscroll_) {
    *handling_event_overscroll_ = std::move(params);
    return;
  }

  delegate_->OnDidOverscroll(*params);
}

}  // namespace content
