// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include <math.h>

#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_disposition_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_sender.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using ui::WebInputEventTraits;

namespace content {
namespace {

const char* GetEventAckName(InputEventAckState ack_result) {
  switch (ack_result) {
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      return "UNKNOWN";
    case INPUT_EVENT_ACK_STATE_CONSUMED:
      return "CONSUMED";
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
      return "NOT_CONSUMED";
    case INPUT_EVENT_ACK_STATE_CONSUMED_SHOULD_BUBBLE:
      return "CONSUMED_SHOULD_BUBBLE";
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      return "NO_CONSUMER_EXISTS";
    case INPUT_EVENT_ACK_STATE_IGNORED:
      return "IGNORED";
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING:
      return "SET_NON_BLOCKING";
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING:
      return "SET_NON_BLOCKING_DUE_TO_FLING";
  }
  DLOG(WARNING) << "Unhandled InputEventAckState in GetEventAckName.";
  return "";
}

bool WasHandled(InputEventAckState state) {
  switch (state) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      return true;
    default:
      return false;
  }
}

ui::WebScopedInputEvent ScaleEvent(const WebInputEvent& event, double scale) {
  std::unique_ptr<blink::WebInputEvent> event_in_viewport =
      ui::ScaleWebInputEvent(event, scale);
  if (event_in_viewport)
    return ui::WebScopedInputEvent(event_in_viewport.release());
  return ui::WebInputEventTraits::Clone(event);
}

}  // namespace

InputRouterImpl::InputRouterImpl(InputRouterImplClient* client,
                                 InputDispositionHandler* disposition_handler,
                                 const Config& config)
    : client_(client),
      disposition_handler_(disposition_handler),
      frame_tree_node_id_(-1),
      active_renderer_fling_count_(0),
      touch_scroll_started_sent_(false),
      wheel_scroll_latching_enabled_(base::FeatureList::IsEnabled(
          features::kTouchpadAndWheelScrollLatching)),
      wheel_event_queue_(this, wheel_scroll_latching_enabled_),
      gesture_event_queue_(this, this, config.gesture_config),
      device_scale_factor_(1.f),
      host_binding_(this),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();

  touch_event_queue_.reset(
      new PassthroughTouchEventQueue(this, config.touch_config));

  DCHECK(client);
  DCHECK(disposition_handler);
  UpdateTouchAckTimeoutEnabled();
}

InputRouterImpl::~InputRouterImpl() {}

void InputRouterImpl::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (mouse_event.event.GetType() == WebInputEvent::kMouseDown &&
      gesture_event_queue_.GetTouchpadTapSuppressionController()
          ->ShouldDeferMouseDown(mouse_event))
    return;
  if (mouse_event.event.GetType() == WebInputEvent::kMouseUp &&
      gesture_event_queue_.GetTouchpadTapSuppressionController()
          ->ShouldSuppressMouseUp())
    return;

  SendMouseEventImmediately(mouse_event);
}

void InputRouterImpl::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  wheel_event_queue_.QueueEvent(wheel_event);
}

void InputRouterImpl::SendKeyboardEvent(
    const NativeWebKeyboardEventWithLatencyInfo& key_event) {
  gesture_event_queue_.FlingHasBeenHalted();
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::KeyboardEventHandled, weak_this_, key_event);
  FilterAndSendWebInputEvent(key_event.event, key_event.latency,
                             std::move(callback));
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& original_gesture_event) {
  input_stream_validator_.Validate(original_gesture_event.event);

  GestureEventWithLatencyInfo gesture_event(original_gesture_event);

  if (touch_action_filter_.FilterGestureEvent(&gesture_event.event))
    return;

  wheel_event_queue_.OnGestureScrollEvent(gesture_event);

  if (gesture_event.event.source_device ==
      blink::kWebGestureDeviceTouchscreen) {
    if (gesture_event.event.GetType() ==
        blink::WebInputEvent::kGestureScrollBegin) {
      touch_scroll_started_sent_ = false;
    } else if (!touch_scroll_started_sent_ &&
               gesture_event.event.GetType() ==
                   blink::WebInputEvent::kGestureScrollUpdate) {
      // A touch scroll hasn't really started until the first
      // GestureScrollUpdate event.  Eg. if the page consumes all touchmoves
      // then no scrolling really ever occurs (even though we still send
      // GestureScrollBegin).
      touch_scroll_started_sent_ = true;
      touch_event_queue_->PrependTouchScrollNotification();
    }
    touch_event_queue_->OnGestureScrollEvent(gesture_event);
  }

  gesture_event_queue_.QueueEvent(gesture_event);
}

void InputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  TouchEventWithLatencyInfo updatd_touch_event = touch_event;
  SetMovementXYForTouchPoints(&updatd_touch_event.event);
  input_stream_validator_.Validate(updatd_touch_event.event);
  touch_event_queue_->QueueEvent(updatd_touch_event);
}

void InputRouterImpl::NotifySiteIsMobileOptimized(bool is_mobile_optimized) {
  touch_event_queue_->SetIsMobileOptimizedSite(is_mobile_optimized);
}

bool InputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_->Empty() || !gesture_event_queue_.empty() ||
         wheel_event_queue_.has_pending() || active_renderer_fling_count_ > 0;
}

void InputRouterImpl::SetDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

void InputRouterImpl::SetFrameTreeNodeId(int frame_tree_node_id) {
  frame_tree_node_id_ = frame_tree_node_id;
}

void InputRouterImpl::SetForceEnableZoom(bool enabled) {
  touch_action_filter_.SetForceEnableZoom(enabled);
}

cc::TouchAction InputRouterImpl::AllowedTouchAction() {
  return touch_action_filter_.allowed_touch_action();
}

void InputRouterImpl::BindHost(mojom::WidgetInputHandlerHostRequest request) {
  host_binding_.Close();
  host_binding_.Bind(std::move(request));
}

void InputRouterImpl::CancelTouchTimeout() {
  touch_event_queue_->SetAckTimeoutEnabled(false);
}

void InputRouterImpl::SetWhiteListedTouchAction(cc::TouchAction touch_action,
                                                uint32_t unique_touch_event_id,
                                                InputEventAckState state) {
  // TODO(hayleyferr): Catch the cases that we have filtered out sending the
  // touchstart.

  touch_action_filter_.OnSetWhiteListedTouchAction(touch_action);
  client_->OnSetWhiteListedTouchAction(touch_action);
}

void InputRouterImpl::DidOverscroll(const ui::DidOverscrollParams& params) {
  client_->DidOverscroll(params);
}

void InputRouterImpl::DidStopFlinging() {
  DCHECK_GT(active_renderer_fling_count_, 0);
  // Note that we're only guaranteed to get a fling end notification from the
  // renderer, not from any other consumers. Consequently, the GestureEventQueue
  // cannot use this bookkeeping for logic like tap suppression.
  --active_renderer_fling_count_;
  client_->DidStopFlinging();
}

void InputRouterImpl::ImeCancelComposition() {
  client_->OnImeCancelComposition();
}

void InputRouterImpl::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& bounds) {
  client_->OnImeCompositionRangeChanged(range, bounds);
}

bool InputRouterImpl::OnMessageReceived(const IPC::Message& message) {
  // TODO(dtapuska): Move these to mojo
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InputRouterImpl, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void InputRouterImpl::SetMovementXYForTouchPoints(blink::WebTouchEvent* event) {
  for (size_t i = 0; i < event->touches_length; ++i) {
    blink::WebTouchPoint* touch_point = &event->touches[i];
    if (touch_point->state == blink::WebTouchPoint::kStateMoved) {
      const gfx::Point& last_position = global_touch_position_[touch_point->id];
      touch_point->movement_x =
          touch_point->PositionInScreen().x - last_position.x();
      touch_point->movement_y =
          touch_point->PositionInScreen().y - last_position.y();
      global_touch_position_[touch_point->id].SetPoint(
          touch_point->PositionInScreen().x, touch_point->PositionInScreen().y);
    } else {
      touch_point->movement_x = 0;
      touch_point->movement_y = 0;
      if (touch_point->state == blink::WebTouchPoint::kStateReleased ||
          touch_point->state == blink::WebTouchPoint::kStateCancelled) {
        global_touch_position_.erase(touch_point->id);
      } else if (touch_point->state == blink::WebTouchPoint::kStatePressed) {
        DCHECK(global_touch_position_.find(touch_point->id) ==
               global_touch_position_.end());
        global_touch_position_[touch_point->id] =
            gfx::Point(touch_point->PositionInScreen().x,
                       touch_point->PositionInScreen().y);
      }
    }
  }
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void InputRouterImpl::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::MouseEventHandled, weak_this_, mouse_event);
  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency,
                             std::move(callback));
}

void InputRouterImpl::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::TouchEventHandled, weak_this_, touch_event);
  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                                      InputEventAckState ack_result) {
  // Touchstart events sent to the renderer indicate a new touch sequence, but
  // in some cases we may filter out sending the touchstart - catch those here.
  if (WebTouchEventTraits::IsTouchSequenceStart(event.event) &&
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    touch_action_filter_.ResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
  disposition_handler_->OnTouchEventAck(event, ack_result);

  // Reset the touch action at the end of a touch-action sequence.
  if (WebTouchEventTraits::IsTouchSequenceEnd(event.event)) {
    touch_action_filter_.ResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
}

void InputRouterImpl::OnFilteringTouchEvent(const WebTouchEvent& touch_event) {
  // The event stream given to the renderer is not guaranteed to be
  // valid based on the current TouchEventStreamValidator rules. This event will
  // never be given to the renderer, but in order to ensure that the event
  // stream |output_stream_validator_| sees is valid, we give events which are
  // filtered out to the validator. crbug.com/589111 proposes adding an
  // additional validator for the events which are actually sent to the
  // renderer.
  output_stream_validator_.Validate(touch_event);
}

void InputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::GestureEventHandled, weak_this_, gesture_event);
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  touch_event_queue_->OnGestureEventAck(event, ack_result);
  disposition_handler_->OnGestureEventAck(event, ack_result);
}

void InputRouterImpl::SendMouseWheelEventImmediately(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  mojom::WidgetInputHandler::DispatchEventCallback callback = base::BindOnce(
      &InputRouterImpl::MouseWheelEventHandled, weak_this_, wheel_event);
  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency,
                             std::move(callback));
}

void InputRouterImpl::OnMouseWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  disposition_handler_->OnWheelEventAck(event, ack_result);
}

void InputRouterImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info) {
  client_->ForwardGestureEventWithLatencyInfo(event, latency_info);
}

void InputRouterImpl::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    mojom::WidgetInputHandler::DispatchEventCallback callback) {
  TRACE_EVENT1("input", "InputRouterImpl::FilterAndSendWebInputEvent", "type",
               WebInputEvent::GetName(input_event.GetType()));
  TRACE_EVENT_WITH_FLOW2(
      "input,benchmark,devtools.timeline", "LatencyInfo.Flow",
      TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SendInputEventUI", "frameTreeNodeId", frame_tree_node_id_);

  output_stream_validator_.Validate(input_event);
  InputEventAckState filtered_state =
      client_->FilterInputEvent(input_event, latency_info);
  if (WasHandled(filtered_state)) {
    if (filtered_state != INPUT_EVENT_ACK_STATE_UNKNOWN) {
      std::move(callback).Run(InputEventAckSource::BROWSER, latency_info,
                              filtered_state, base::nullopt, base::nullopt);
    }
    return;
  }

  std::unique_ptr<InputEvent> event = base::MakeUnique<InputEvent>(
      ScaleEvent(input_event, device_scale_factor_), latency_info);
  if (WebInputEventTraits::ShouldBlockEventStream(
          input_event, wheel_scroll_latching_enabled_)) {
    client_->IncrementInFlightEventCount(input_event.GetType());
    client_->GetWidgetInputHandler()->DispatchEvent(std::move(event),
                                                    std::move(callback));
  } else {
    client_->GetWidgetInputHandler()->DispatchNonBlockingEvent(
        std::move(event));
    std::move(callback).Run(InputEventAckSource::BROWSER, latency_info,
                            INPUT_EVENT_ACK_STATE_IGNORED, base::nullopt,
                            base::nullopt);
  }
}

void InputRouterImpl::KeyboardEventHandled(
    const NativeWebKeyboardEventWithLatencyInfo& event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::KeboardEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               GetEventAckName(state));

  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);
  disposition_handler_->OnKeyboardEventAck(event, state);

  // WARNING: This InputRouterImpl can be deallocated at this point
  // (i.e.  in the case of Ctrl+W, where the call to
  // HandleKeyboardEvent destroys this InputRouterImpl).
  // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
}

void InputRouterImpl::MouseEventHandled(
    const MouseEventWithLatencyInfo& event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::MouseEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               GetEventAckName(state));

  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);
  disposition_handler_->OnMouseEventAck(event, state);
}

void InputRouterImpl::TouchEventHandled(
    const TouchEventWithLatencyInfo& touch_event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::TouchEventHandled", "type",
               WebInputEvent::GetName(touch_event.event.GetType()), "ack",
               GetEventAckName(state));
  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  touch_event.latency.AddNewLatencyFrom(latency);

  // The SetTouchAction IPC occurs on a different channel so always
  // send it in the input event ack to ensure it is available at the
  // time the ACK is handled.
  if (touch_action.has_value())
    OnSetTouchAction(touch_action.value());

  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_->ProcessTouchAck(state, latency,
                                      touch_event.event.unique_touch_event_id);
}

void InputRouterImpl::GestureEventHandled(
    const GestureEventWithLatencyInfo& gesture_event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::GestureEventHandled", "type",
               WebInputEvent::GetName(gesture_event.event.GetType()), "ack",
               GetEventAckName(state));
  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  if (gesture_event.event.GetType() ==
          blink::WebInputEvent::kGestureFlingStart &&
      state == INPUT_EVENT_ACK_STATE_CONSUMED) {
    ++active_renderer_fling_count_;
  }

  if (overscroll) {
    DCHECK_EQ(WebInputEvent::kGestureScrollUpdate,
              gesture_event.event.GetType());
    DidOverscroll(overscroll.value());
  }

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(state, gesture_event.event.GetType(),
                                         latency);
}

void InputRouterImpl::MouseWheelEventHandled(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckSource source,
    const ui::LatencyInfo& latency,
    InputEventAckState state,
    const base::Optional<ui::DidOverscrollParams>& overscroll,
    const base::Optional<cc::TouchAction>& touch_action) {
  TRACE_EVENT2("input", "InputRouterImpl::MouseWheelEventHandled", "type",
               WebInputEvent::GetName(event.event.GetType()), "ack",
               GetEventAckName(state));
  if (source != InputEventAckSource::BROWSER)
    client_->DecrementInFlightEventCount(source);
  event.latency.AddNewLatencyFrom(latency);

  if (overscroll)
    DidOverscroll(overscroll.value());

  wheel_event_queue_.ProcessMouseWheelAck(state, event.latency);
}

void InputRouterImpl::OnHasTouchEventHandlers(bool has_handlers) {
  TRACE_EVENT1("input", "InputRouterImpl::OnHasTouchEventHandlers",
               "has_handlers", has_handlers);

  // Lack of a touch handler indicates that the page either has no touch-action
  // modifiers or that all its touch-action modifiers are auto. Resetting the
  // touch-action here allows forwarding of subsequent gestures even if the
  // underlying touches never reach the router.
  if (!has_handlers)
    touch_action_filter_.ResetTouchAction();

  touch_event_queue_->OnHasTouchEventHandlers(has_handlers);
  client_->OnHasTouchEventHandlers(has_handlers);
}

void InputRouterImpl::OnSetTouchAction(cc::TouchAction touch_action) {
  // Synthetic touchstart events should get filtered out in RenderWidget.
  DCHECK(touch_event_queue_->IsPendingAckTouchStart());
  TRACE_EVENT1("input", "InputRouterImpl::OnSetTouchAction", "action",
               touch_action);

  touch_action_filter_.OnSetTouchAction(touch_action);

  // kTouchActionNone should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
}

void InputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  // kTouchActionNone will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  const bool touch_ack_timeout_enabled =
      touch_action_filter_.allowed_touch_action() != cc::kTouchActionNone;
  touch_event_queue_->SetAckTimeoutEnabled(touch_ack_timeout_enabled);
}

}  // namespace content
