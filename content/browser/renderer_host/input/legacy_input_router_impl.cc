// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/legacy_input_router_impl.h"

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

}  // namespace

LegacyInputRouterImpl::LegacyInputRouterImpl(
    IPC::Sender* sender,
    InputRouterClient* client,
    InputDispositionHandler* disposition_handler,
    int routing_id,
    const Config& config)
    : sender_(sender),
      client_(client),
      disposition_handler_(disposition_handler),
      routing_id_(routing_id),
      frame_tree_node_id_(-1),
      select_message_pending_(false),
      move_caret_pending_(false),
      active_renderer_fling_count_(0),
      touch_scroll_started_sent_(false),
      wheel_scroll_latching_enabled_(base::FeatureList::IsEnabled(
          features::kTouchpadAndWheelScrollLatching)),
      wheel_event_queue_(this, wheel_scroll_latching_enabled_),
      gesture_event_queue_(this, this, config.gesture_config),
      device_scale_factor_(1.f) {
  touch_event_queue_.reset(
      new PassthroughTouchEventQueue(this, config.touch_config));

  DCHECK(sender);
  DCHECK(client);
  DCHECK(disposition_handler);
  UpdateTouchAckTimeoutEnabled();
}

LegacyInputRouterImpl::~LegacyInputRouterImpl() {}

bool LegacyInputRouterImpl::SendInput(std::unique_ptr<IPC::Message> message) {
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart);
  switch (message->type()) {
    // Check for types that require an ACK.
    case InputMsg_SelectRange::ID:
    case InputMsg_MoveRangeSelectionExtent::ID:
      return SendSelectMessage(std::move(message));
    case InputMsg_MoveCaret::ID:
      return SendMoveCaret(std::move(message));
    case InputMsg_HandleInputEvent::ID:
      NOTREACHED() << "WebInputEvents should never be sent via SendInput.";
      return false;
    default:
      return Send(message.release());
  }
}

void LegacyInputRouterImpl::SendMouseEvent(
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

void LegacyInputRouterImpl::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  wheel_event_queue_.QueueEvent(wheel_event);
}

void LegacyInputRouterImpl::SendKeyboardEvent(
    const NativeWebKeyboardEventWithLatencyInfo& key_event) {
  // Put all WebKeyboardEvent objects in a queue since we can't trust the
  // renderer and we need to give something to the HandleKeyboardEvent
  // handler.
  key_queue_.push_back(key_event);
  LOCAL_HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

  gesture_event_queue_.FlingHasBeenHalted();

  // Only forward the non-native portions of our event.
  FilterAndSendWebInputEvent(key_event.event, key_event.latency);
}

void LegacyInputRouterImpl::SendGestureEvent(
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

void LegacyInputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  TouchEventWithLatencyInfo updatd_touch_event = touch_event;
  SetMovementXYForTouchPoints(&updatd_touch_event.event);
  input_stream_validator_.Validate(updatd_touch_event.event);
  touch_event_queue_->QueueEvent(updatd_touch_event);
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void LegacyInputRouterImpl::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event) {
  mouse_event_queue_.push_back(mouse_event);
  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency);
}

void LegacyInputRouterImpl::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency);
}

void LegacyInputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency);
}

void LegacyInputRouterImpl::NotifySiteIsMobileOptimized(
    bool is_mobile_optimized) {
  touch_event_queue_->SetIsMobileOptimizedSite(is_mobile_optimized);
}

bool LegacyInputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_->Empty() || !gesture_event_queue_.empty() ||
         !key_queue_.empty() || !mouse_event_queue_.empty() ||
         wheel_event_queue_.has_pending() || select_message_pending_ ||
         move_caret_pending_ || active_renderer_fling_count_ > 0;
}

void LegacyInputRouterImpl::SetDeviceScaleFactor(float device_scale_factor) {
  device_scale_factor_ = device_scale_factor;
}

void LegacyInputRouterImpl::BindHost(
    mojom::WidgetInputHandlerHostRequest request) {
  NOTREACHED();
}

bool LegacyInputRouterImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(LegacyInputRouterImpl, message)
    IPC_MESSAGE_HANDLER(InputHostMsg_HandleInputEvent_ACK, OnInputEventAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_DidOverscroll, OnDidOverscroll)
    IPC_MESSAGE_HANDLER(InputHostMsg_MoveCaret_ACK, OnMsgMoveCaretAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_SelectRange_ACK, OnSelectMessageAck)
    IPC_MESSAGE_HANDLER(InputHostMsg_MoveRangeSelectionExtent_ACK,
                        OnSelectMessageAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(InputHostMsg_SetTouchAction, OnSetTouchAction)
    IPC_MESSAGE_HANDLER(InputHostMsg_SetWhiteListedTouchAction,
                        OnSetWhiteListedTouchAction)
    IPC_MESSAGE_HANDLER(InputHostMsg_DidStopFlinging, OnDidStopFlinging)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void LegacyInputRouterImpl::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  // Touchstart events sent to the renderer indicate a new touch sequence, but
  // in some cases we may filter out sending the touchstart - catch those here.
  if (WebTouchEventTraits::IsTouchSequenceStart(event.event) &&
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    touch_action_filter_.ResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
  disposition_handler_->OnTouchEventAck(event, ack_source, ack_result);

  // Reset the touch action at the end of a touch-action sequence.
  if (WebTouchEventTraits::IsTouchSequenceEnd(event.event)) {
    touch_action_filter_.ReportAndResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
}

void LegacyInputRouterImpl::OnFilteringTouchEvent(
    const WebTouchEvent& touch_event) {
  // The event stream given to the renderer is not guaranteed to be
  // valid based on the current TouchEventStreamValidator rules. This event will
  // never be given to the renderer, but in order to ensure that the event
  // stream |output_stream_validator_| sees is valid, we give events which are
  // filtered out to the validator. crbug.com/589111 proposes adding an
  // additional validator for the events which are actually sent to the
  // renderer.
  output_stream_validator_.Validate(touch_event);
}

void LegacyInputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  touch_event_queue_->OnGestureEventAck(event, ack_result);
  disposition_handler_->OnGestureEventAck(event, ack_source, ack_result);
}

void LegacyInputRouterImpl::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info) {
  client_->ForwardGestureEventWithLatencyInfo(event, latency_info);
}

void LegacyInputRouterImpl::SendMouseWheelEventImmediately(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency);
}

void LegacyInputRouterImpl::OnMouseWheelEventAck(
    const MouseWheelEventWithLatencyInfo& event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  disposition_handler_->OnWheelEventAck(event, ack_source, ack_result);
}

bool LegacyInputRouterImpl::SendSelectMessage(
    std::unique_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_SelectRange::ID ||
         message->type() == InputMsg_MoveRangeSelectionExtent::ID);

  // TODO(jdduke): Factor out common logic between selection and caret-related
  //               IPC messages.
  if (select_message_pending_) {
    if (!pending_select_messages_.empty() &&
        pending_select_messages_.back()->type() == message->type()) {
      pending_select_messages_.pop_back();
    }

    pending_select_messages_.push_back(std::move(message));
    return true;
  }

  select_message_pending_ = true;
  return Send(message.release());
}

bool LegacyInputRouterImpl::SendMoveCaret(
    std::unique_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_MoveCaret::ID);
  if (move_caret_pending_) {
    next_move_caret_ = std::move(message);
    return true;
  }

  move_caret_pending_ = true;
  return Send(message.release());
}

bool LegacyInputRouterImpl::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void LegacyInputRouterImpl::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info) {
  TRACE_EVENT1("input", "LegacyInputRouterImpl::FilterAndSendWebInputEvent",
               "type", WebInputEvent::GetName(input_event.GetType()));
  TRACE_EVENT_WITH_FLOW2(
      "input,benchmark,devtools.timeline", "LatencyInfo.Flow",
      TRACE_ID_DONT_MANGLE(latency_info.trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SendInputEventUI", "frameTreeNodeId", frame_tree_node_id_);

  OfferToHandlers(input_event, latency_info);
}

void LegacyInputRouterImpl::OfferToHandlers(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info) {
  output_stream_validator_.Validate(input_event);

  if (OfferToClient(input_event, latency_info))
    return;

  bool should_block = WebInputEventTraits::ShouldBlockEventStream(
      input_event, wheel_scroll_latching_enabled_);
  OfferToRenderer(input_event, latency_info,
                  should_block
                      ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
                      : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING);

  // Generate a synthetic ack if the event was sent so it doesn't block.
  if (!should_block) {
    ProcessInputEventAck(
        input_event.GetType(), InputEventAckSource::BROWSER,
        INPUT_EVENT_ACK_STATE_IGNORED, latency_info,
        WebInputEventTraits::GetUniqueTouchEventId(input_event));
  }
}

bool LegacyInputRouterImpl::OfferToClient(const WebInputEvent& input_event,
                                          const ui::LatencyInfo& latency_info) {
  bool consumed = false;

  InputEventAckState filter_ack =
      client_->FilterInputEvent(input_event, latency_info);
  switch (filter_ack) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      // Send the ACK and early exit.
      ProcessInputEventAck(
          input_event.GetType(), InputEventAckSource::BROWSER, filter_ack,
          latency_info,
          WebInputEventTraits::GetUniqueTouchEventId(input_event));
      // WARNING: |this| may be deleted at this point.
      consumed = true;
      break;
    case INPUT_EVENT_ACK_STATE_UNKNOWN:
      // Simply drop the event.
      consumed = true;
      break;
    default:
      break;
  }

  return consumed;
}

bool LegacyInputRouterImpl::OfferToRenderer(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  // This conversion is temporary. WebInputEvent should be generated
  // directly from ui::Event with the viewport coordinates. See
  // crbug.com/563730.
  std::unique_ptr<blink::WebInputEvent> event_in_viewport =
      ui::ScaleWebInputEvent(input_event, device_scale_factor_);
  const WebInputEvent* event_to_send =
      event_in_viewport ? event_in_viewport.get() : &input_event;

  if (Send(new InputMsg_HandleInputEvent(
          routing_id(), event_to_send, std::vector<IPC::WebInputEventPointer>(),
          latency_info, dispatch_type))) {
    // Ack messages for ignored ack event types should never be sent by the
    // renderer. Consequently, such event types should not affect event time
    // or in-flight event count metrics.
    if (dispatch_type == InputEventDispatchType::DISPATCH_TYPE_BLOCKING)
      client_->IncrementInFlightEventCount(input_event.GetType());
    return true;
  }
  return false;
}

void LegacyInputRouterImpl::OnInputEventAck(const InputEventAck& ack) {
  client_->DecrementInFlightEventCount(ack.source);

  if (ack.overscroll) {
    DCHECK(ack.type == WebInputEvent::kMouseWheel ||
           ack.type == WebInputEvent::kGestureScrollUpdate);
    OnDidOverscroll(*ack.overscroll);
  }

  // Since input messages over mojo require the touch action in the
  // ACK we mirror that behavior in Chrome IPC for simplicity.
  if (ack.touch_action.has_value())
    OnSetTouchAction(ack.touch_action.value());

  ProcessInputEventAck(ack.type, ack.source, ack.state, ack.latency,
                       ack.unique_touch_event_id);
}

void LegacyInputRouterImpl::OnDidOverscroll(
    const ui::DidOverscrollParams& params) {
  client_->DidOverscroll(params);
}

void LegacyInputRouterImpl::OnMsgMoveCaretAck() {
  move_caret_pending_ = false;
  if (next_move_caret_)
    SendMoveCaret(std::move(next_move_caret_));
}

void LegacyInputRouterImpl::OnSelectMessageAck() {
  select_message_pending_ = false;
  if (!pending_select_messages_.empty()) {
    std::unique_ptr<IPC::Message> next_message =
        std::move(pending_select_messages_.front());
    pending_select_messages_.pop_front();

    SendSelectMessage(std::move(next_message));
  }
}

void LegacyInputRouterImpl::OnHasTouchEventHandlers(bool has_handlers) {
  TRACE_EVENT1("input", "LegacyInputRouterImpl::OnHasTouchEventHandlers",
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

void LegacyInputRouterImpl::OnSetTouchAction(cc::TouchAction touch_action) {
  // Synthetic touchstart events should get filtered out in RenderWidget.
  DCHECK(touch_event_queue_->IsPendingAckTouchStart());
  TRACE_EVENT1("input", "LegacyInputRouterImpl::OnSetTouchAction", "action",
               touch_action);

  touch_action_filter_.OnSetTouchAction(touch_action);

  // kTouchActionNone should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
}

void LegacyInputRouterImpl::OnSetWhiteListedTouchAction(
    cc::TouchAction white_listed_touch_action,
    uint32_t unique_touch_event_id,
    InputEventAckState ack_result) {
  // TODO(hayleyferr): Catch the cases that we have filtered out sending the
  // touchstart.

  touch_action_filter_.OnSetWhiteListedTouchAction(white_listed_touch_action);
  client_->OnSetWhiteListedTouchAction(white_listed_touch_action);
}

void LegacyInputRouterImpl::OnDidStopFlinging() {
  DCHECK_GT(active_renderer_fling_count_, 0);
  // Note that we're only guaranteed to get a fling end notification from the
  // renderer, not from any other consumers. Consequently, the GestureEventQueue
  // cannot use this bookkeeping for logic like tap suppression.
  --active_renderer_fling_count_;

  client_->DidStopFlinging();
}

void LegacyInputRouterImpl::ProcessInputEventAck(
    WebInputEvent::Type event_type,
    InputEventAckSource ack_source,
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency_info,
    uint32_t unique_touch_event_id) {
  TRACE_EVENT2("input", "LegacyInputRouterImpl::ProcessInputEventAck", "type",
               WebInputEvent::GetName(event_type), "ack",
               GetEventAckName(ack_result));

  if (WebInputEvent::IsKeyboardEventType(event_type)) {
    // Note: The keyboard ack must be treated carefully, as it may result in
    // synchronous destruction of |this|. Handling immediately guards against
    // future references to |this|.
    ProcessKeyboardAck(event_type, ack_source, ack_result, latency_info);
    // WARNING: |this| may be deleted at this point.
    return;
  } else if (WebInputEvent::IsMouseEventType(event_type)) {
    ProcessMouseAck(event_type, ack_source, ack_result, latency_info);
  } else if (event_type == WebInputEvent::kMouseWheel) {
    ProcessWheelAck(ack_source, ack_result, latency_info);
  } else if (WebInputEvent::IsTouchEventType(event_type)) {
    ProcessTouchAck(ack_source, ack_result, latency_info,
                    unique_touch_event_id);
  } else if (WebInputEvent::IsGestureEventType(event_type)) {
    ProcessGestureAck(event_type, ack_source, ack_result, latency_info);
  } else if (event_type != WebInputEvent::kUndefined) {
    disposition_handler_->OnUnexpectedEventAck(
        InputDispositionHandler::BAD_ACK_MESSAGE);
  }
}

void LegacyInputRouterImpl::ProcessKeyboardAck(blink::WebInputEvent::Type type,
                                               InputEventAckSource ack_source,
                                               InputEventAckState ack_result,
                                               const ui::LatencyInfo& latency) {
  if (key_queue_.empty()) {
    disposition_handler_->OnUnexpectedEventAck(
        InputDispositionHandler::UNEXPECTED_ACK);
  } else if (key_queue_.front().event.GetType() != type) {
    // Something must be wrong. Clear the |key_queue_| and char event
    // suppression so that we can resume from the error.
    key_queue_.clear();
    disposition_handler_->OnUnexpectedEventAck(
        InputDispositionHandler::UNEXPECTED_EVENT_TYPE);
  } else {
    NativeWebKeyboardEventWithLatencyInfo front_item = key_queue_.front();
    front_item.latency.AddNewLatencyFrom(latency);
    key_queue_.pop_front();

    disposition_handler_->OnKeyboardEventAck(front_item, ack_source,
                                             ack_result);
    // WARNING: This LegacyInputRouterImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this LegacyInputRouterImpl).
    // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
  }
}

void LegacyInputRouterImpl::ProcessMouseAck(blink::WebInputEvent::Type type,
                                            InputEventAckSource ack_source,
                                            InputEventAckState ack_result,
                                            const ui::LatencyInfo& latency) {
  if (mouse_event_queue_.empty()) {
    disposition_handler_->OnUnexpectedEventAck(
        InputDispositionHandler::UNEXPECTED_ACK);
  } else {
    MouseEventWithLatencyInfo front_item = mouse_event_queue_.front();
    front_item.latency.AddNewLatencyFrom(latency);
    mouse_event_queue_.pop_front();
    disposition_handler_->OnMouseEventAck(front_item, ack_source, ack_result);
  }
}

void LegacyInputRouterImpl::ProcessWheelAck(InputEventAckSource ack_source,
                                            InputEventAckState ack_result,
                                            const ui::LatencyInfo& latency) {
  wheel_event_queue_.ProcessMouseWheelAck(ack_source, ack_result, latency);
}

void LegacyInputRouterImpl::ProcessGestureAck(WebInputEvent::Type type,
                                              InputEventAckSource ack_source,
                                              InputEventAckState ack_result,
                                              const ui::LatencyInfo& latency) {
  if (type == blink::WebInputEvent::kGestureFlingStart &&
      ack_result == INPUT_EVENT_ACK_STATE_CONSUMED) {
    ++active_renderer_fling_count_;
  }

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(ack_source, ack_result, type, latency);
}

void LegacyInputRouterImpl::ProcessTouchAck(InputEventAckSource ack_source,
                                            InputEventAckState ack_result,
                                            const ui::LatencyInfo& latency,
                                            uint32_t unique_touch_event_id) {
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_->ProcessTouchAck(ack_source, ack_result, latency,
                                      unique_touch_event_id);
}

void LegacyInputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  // kTouchActionNone will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  const bool touch_ack_timeout_enabled =
      touch_action_filter_.allowed_touch_action() != cc::kTouchActionNone;
  touch_event_queue_->SetAckTimeoutEnabled(touch_ack_timeout_enabled);
}

void LegacyInputRouterImpl::SetFrameTreeNodeId(int frameTreeNodeId) {
  frame_tree_node_id_ = frameTreeNodeId;
}

cc::TouchAction LegacyInputRouterImpl::AllowedTouchAction() {
  return touch_action_filter_.allowed_touch_action();
}

void LegacyInputRouterImpl::SetForceEnableZoom(bool enabled) {
  touch_action_filter_.SetForceEnableZoom(enabled);
}

void LegacyInputRouterImpl::SetMovementXYForTouchPoints(
    blink::WebTouchEvent* event) {
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

}  // namespace content
