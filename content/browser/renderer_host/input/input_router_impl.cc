// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_filter.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/touch_action.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_sender.h"
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

namespace content {
namespace {

bool GetTouchAckTimeoutDelayMs(size_t* touch_ack_timeout_delay_ms) {
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();
  if (!parsed_command_line->HasSwitch(switches::kTouchAckTimeoutDelayMs))
    return false;

  std::string timeout_string = parsed_command_line->GetSwitchValueASCII(
      switches::kTouchAckTimeoutDelayMs);
  size_t timeout_value;
  if (!base::StringToSizeT(timeout_string, &timeout_value))
    return false;

  *touch_ack_timeout_delay_ms = timeout_value;
  return true;
}

GestureEventWithLatencyInfo MakeGestureEvent(WebInputEvent::Type type,
                                             double timestamp_seconds,
                                             int x,
                                             int y,
                                             int modifiers,
                                             const ui::LatencyInfo latency) {
  WebGestureEvent result;

  result.type = type;
  result.x = x;
  result.y = y;
  result.sourceDevice = WebGestureEvent::Touchscreen;
  result.timeStampSeconds = timestamp_seconds;
  result.modifiers = modifiers;

  return GestureEventWithLatencyInfo(result, latency);
}

const char* GetEventAckName(InputEventAckState ack_result) {
  switch(ack_result) {
    case INPUT_EVENT_ACK_STATE_UNKNOWN: return "UNKNOWN";
    case INPUT_EVENT_ACK_STATE_CONSUMED: return "CONSUMED";
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED: return "NOT_CONSUMED";
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS: return "NO_CONSUMER_EXISTS";
    case INPUT_EVENT_ACK_STATE_IGNORED: return "IGNORED";
  }
  DLOG(WARNING) << "Unhandled InputEventAckState in GetEventAckName.";
  return "";
}

} // namespace

InputRouterImpl::InputRouterImpl(IPC::Sender* sender,
                                 InputRouterClient* client,
                                 InputAckHandler* ack_handler,
                                 int routing_id)
    : sender_(sender),
      client_(client),
      ack_handler_(ack_handler),
      routing_id_(routing_id),
      select_range_pending_(false),
      move_caret_pending_(false),
      mouse_move_pending_(false),
      mouse_wheel_pending_(false),
      has_touch_handler_(false),
      touch_ack_timeout_enabled_(false),
      touch_ack_timeout_delay_ms_(std::numeric_limits<size_t>::max()),
      current_ack_source_(ACK_SOURCE_NONE),
      gesture_event_filter_(new GestureEventFilter(this, this)) {
  DCHECK(sender);
  DCHECK(client);
  DCHECK(ack_handler);
  touch_event_queue_.reset(new TouchEventQueue(this));
  touch_ack_timeout_enabled_ =
      GetTouchAckTimeoutDelayMs(&touch_ack_timeout_delay_ms_);
  touch_event_queue_->SetAckTimeoutEnabled(touch_ack_timeout_enabled_,
                                           touch_ack_timeout_delay_ms_);
}

InputRouterImpl::~InputRouterImpl() {}

void InputRouterImpl::Flush() {}

bool InputRouterImpl::SendInput(scoped_ptr<IPC::Message> message) {
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart);
  switch (message->type()) {
    // Check for types that require an ACK.
    case InputMsg_SelectRange::ID:
      return SendSelectRange(message.Pass());
    case InputMsg_MoveCaret::ID:
      return SendMoveCaret(message.Pass());
    case InputMsg_HandleInputEvent::ID:
      NOTREACHED() << "WebInputEvents should never be sent via SendInput.";
      return false;
    default:
      return Send(message.release());
  }
}

void InputRouterImpl::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event) {
  // Order is important here; we need to convert all MouseEvents before they
  // propagate further, e.g., to the tap suppression controller.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSimulateTouchScreenWithMouse)) {
    SimulateTouchGestureWithMouse(mouse_event);
    return;
  }

  if (mouse_event.event.type == WebInputEvent::MouseDown &&
      gesture_event_filter_->GetTouchpadTapSuppressionController()->
          ShouldDeferMouseDown(mouse_event))
      return;
  if (mouse_event.event.type == WebInputEvent::MouseUp &&
      gesture_event_filter_->GetTouchpadTapSuppressionController()->
          ShouldSuppressMouseUp())
      return;

  SendMouseEventImmediately(mouse_event);
}

void InputRouterImpl::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  // If there's already a mouse wheel event waiting to be sent to the renderer,
  // add the new deltas to that event. Not doing so (e.g., by dropping the old
  // event, as for mouse moves) results in very slow scrolling on the Mac (on
  // which many, very small wheel events are sent).
  if (mouse_wheel_pending_) {
    if (coalesced_mouse_wheel_events_.empty() ||
        !coalesced_mouse_wheel_events_.back().CanCoalesceWith(wheel_event)) {
      coalesced_mouse_wheel_events_.push_back(wheel_event);
    } else {
      coalesced_mouse_wheel_events_.back().CoalesceWith(wheel_event);
    }
    return;
  }
  mouse_wheel_pending_ = true;
  current_wheel_event_ = wheel_event;

  HISTOGRAM_COUNTS_100("Renderer.WheelQueueSize",
                       coalesced_mouse_wheel_events_.size());

  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency, false);
}

void InputRouterImpl::SendKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                                        const ui::LatencyInfo& latency_info,
                                        bool is_keyboard_shortcut) {
  // Put all WebKeyboardEvent objects in a queue since we can't trust the
  // renderer and we need to give something to the HandleKeyboardEvent
  // handler.
  key_queue_.push_back(key_event);
  HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

  gesture_event_filter_->FlingHasBeenHalted();

  // Only forward the non-native portions of our event.
  FilterAndSendWebInputEvent(key_event, latency_info, is_keyboard_shortcut);
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (touch_action_filter_.FilterGestureEvent(gesture_event.event))
    return;

  touch_event_queue_->OnGestureScrollEvent(gesture_event);

  if (!IsInOverscrollGesture() &&
      !gesture_event_filter_->ShouldForward(gesture_event)) {
    OverscrollController* controller = client_->GetOverscrollController();
    if (controller)
      controller->DiscardingGestureEvent(gesture_event.event);
    return;
  }

  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency, false);
}

void InputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  touch_event_queue_->QueueEvent(touch_event);
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void InputRouterImpl::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event) {
  // Avoid spamming the renderer with mouse move events.  It is important
  // to note that WM_MOUSEMOVE events are anyways synthetic, but since our
  // thread is able to rapidly consume WM_MOUSEMOVE events, we may get way
  // more WM_MOUSEMOVE events than we wish to send to the renderer.
  if (mouse_event.event.type == WebInputEvent::MouseMove) {
    if (mouse_move_pending_) {
      if (!next_mouse_move_)
        next_mouse_move_.reset(new MouseEventWithLatencyInfo(mouse_event));
      else
        next_mouse_move_->CoalesceWith(mouse_event);
      return;
    }
    mouse_move_pending_ = true;
  }

  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency, false);
}

void InputRouterImpl::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency, false);
}

void InputRouterImpl::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency, false);
}

const NativeWebKeyboardEvent* InputRouterImpl::GetLastKeyboardEvent() const {
  if (key_queue_.empty())
    return NULL;
  return &key_queue_.front();
}

bool InputRouterImpl::ShouldForwardTouchEvent() const {
  // Always send a touch event if the renderer has a touch-event handler. It is
  // possible that a renderer stops listening to touch-events while there are
  // still events in the touch-queue. In such cases, the new events should still
  // get into the queue.
  return has_touch_handler_ || !touch_event_queue_->empty();
}

void InputRouterImpl::OnViewUpdated(int view_flags) {
  bool fixed_page_scale = (view_flags & FIXED_PAGE_SCALE) != 0;
  bool mobile_viewport = (view_flags & MOBILE_VIEWPORT) != 0;
  touch_event_queue_->SetAckTimeoutEnabled(
      touch_ack_timeout_enabled_ && !(fixed_page_scale || mobile_viewport),
      touch_ack_timeout_delay_ms_);
}

bool InputRouterImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(InputRouterImpl, message, message_is_ok)
    IPC_MESSAGE_HANDLER(InputHostMsg_HandleInputEvent_ACK, OnInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MoveCaret_ACK, OnMsgMoveCaretAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectRange_ACK, OnSelectRangeAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_HANDLER(InputHostMsg_SetTouchAction,
                        OnSetTouchAction)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!message_is_ok)
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::BAD_ACK_MESSAGE);

  return handled;
}

void InputRouterImpl::OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                                      InputEventAckState ack_result) {
  ack_handler_->OnTouchEventAck(event, ack_result);
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  ProcessAckForOverscroll(event.event, ack_result);
  ack_handler_->OnGestureEventAck(event, ack_result);
}

bool InputRouterImpl::SendSelectRange(scoped_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_SelectRange::ID);
  if (select_range_pending_) {
    next_selection_range_ = message.Pass();
    return true;
  }

  select_range_pending_ = true;
  return Send(message.release());
}

bool InputRouterImpl::SendMoveCaret(scoped_ptr<IPC::Message> message) {
  DCHECK(message->type() == InputMsg_MoveCaret::ID);
  if (move_caret_pending_) {
    next_move_caret_ = message.Pass();
    return true;
  }

  move_caret_pending_ = true;
  return Send(message.release());
}

bool InputRouterImpl::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void InputRouterImpl::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    bool is_keyboard_shortcut) {
  TRACE_EVENT1("input",
               "InputRouterImpl::FilterAndSendWebInputEvent",
               "type",
               WebInputEventTraits::GetName(input_event.type));

  // Transmit any pending wheel events on a non-wheel event. This ensures that
  // final PhaseEnded wheel event is received, which is necessary to terminate
  // rubber-banding, for example.
   if (input_event.type != WebInputEvent::MouseWheel) {
    WheelEventQueue mouse_wheel_events;
    mouse_wheel_events.swap(coalesced_mouse_wheel_events_);
    for (size_t i = 0; i < mouse_wheel_events.size(); ++i) {
      OfferToHandlers(mouse_wheel_events[i].event,
                      mouse_wheel_events[i].latency,
                      false);
     }
  }

  // Any input event cancels a pending mouse move event.
  next_mouse_move_.reset();

  OfferToHandlers(input_event, latency_info, is_keyboard_shortcut);
}

void InputRouterImpl::OfferToHandlers(const WebInputEvent& input_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  if (OfferToOverscrollController(input_event, latency_info))
    return;

  if (OfferToClient(input_event, latency_info))
    return;

  OfferToRenderer(input_event, latency_info, is_keyboard_shortcut);

  // If we don't care about the ack disposition, send the ack immediately.
  if (WebInputEventTraits::IgnoresAckDisposition(input_event.type)) {
    ProcessInputEventAck(input_event.type,
                         INPUT_EVENT_ACK_STATE_IGNORED,
                         latency_info,
                         IGNORING_DISPOSITION);
  }
}

bool InputRouterImpl::OfferToOverscrollController(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info) {
  OverscrollController* controller = client_->GetOverscrollController();
  if (!controller)
    return false;

  OverscrollController::Disposition disposition =
      controller->DispatchEvent(input_event, latency_info);

  bool consumed = disposition == OverscrollController::CONSUMED;

  if (disposition == OverscrollController::SHOULD_FORWARD_TO_GESTURE_FILTER) {
    DCHECK(WebInputEvent::isGestureEventType(input_event.type));
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // An ACK is expected for the event, so mark it as consumed.
    consumed = !gesture_event_filter_->ShouldForward(
        GestureEventWithLatencyInfo(gesture_event, latency_info));
  }

  if (consumed) {
    InputEventAckState overscroll_ack =
        WebInputEvent::isTouchEventType(input_event.type) ?
            INPUT_EVENT_ACK_STATE_NOT_CONSUMED : INPUT_EVENT_ACK_STATE_CONSUMED;
    ProcessInputEventAck(input_event.type,
                         overscroll_ack,
                         latency_info,
                         OVERSCROLL_CONTROLLER);
    // WARNING: |this| may be deleted at this point.
  }

  return consumed;
}

bool InputRouterImpl::OfferToClient(const WebInputEvent& input_event,
                                    const ui::LatencyInfo& latency_info) {
  bool consumed = false;

  InputEventAckState filter_ack =
      client_->FilterInputEvent(input_event, latency_info);
  switch (filter_ack) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      // Send the ACK and early exit.
      next_mouse_move_.reset();
      ProcessInputEventAck(input_event.type, filter_ack, latency_info, CLIENT);
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

bool InputRouterImpl::OfferToRenderer(const WebInputEvent& input_event,
                                      const ui::LatencyInfo& latency_info,
                                      bool is_keyboard_shortcut) {
  input_event_start_time_ = TimeTicks::Now();
  if (Send(new InputMsg_HandleInputEvent(
          routing_id(), &input_event, latency_info, is_keyboard_shortcut))) {
    // Only increment the event count if we require an ACK for |input_event|.
    if (!WebInputEventTraits::IgnoresAckDisposition(input_event.type))
      client_->IncrementInFlightEventCount();
    return true;
  }
  return false;
}

void InputRouterImpl::OnInputEventAck(WebInputEvent::Type event_type,
                                      InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency_info) {
  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.IIR_InputEventDelta", delta);

  // A synthetic ack will already have been sent for this event, and it will
  // not have affected the in-flight event count.
  if (WebInputEventTraits::IgnoresAckDisposition(event_type))
    return;

  client_->DecrementInFlightEventCount();

  ProcessInputEventAck(event_type, ack_result, latency_info, RENDERER);
  // WARNING: |this| may be deleted at this point.

  // This is used only for testing, and the other end does not use the
  // source object.  On linux, specifying
  // Source<RenderWidgetHost> results in a very strange
  // runtime error in the epilogue of the enclosing
  // (ProcessInputEventAck) method, but not on other platforms; using
  // 'void' instead is just as safe (since NotificationSource
  // is not actually typesafe) and avoids this error.
  int type = static_cast<int>(event_type);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,
      Source<void>(this),
      Details<int>(&type));
}

void InputRouterImpl::OnMsgMoveCaretAck() {
  move_caret_pending_ = false;
  if (next_move_caret_)
    SendMoveCaret(next_move_caret_.Pass());
}

void InputRouterImpl::OnSelectRangeAck() {
  select_range_pending_ = false;
  if (next_selection_range_)
    SendSelectRange(next_selection_range_.Pass());
}

void InputRouterImpl::OnHasTouchEventHandlers(bool has_handlers) {
 if (has_touch_handler_ == has_handlers)
    return;
  has_touch_handler_ = has_handlers;
  if (!has_handlers)
    touch_event_queue_->FlushQueue();
  client_->OnHasTouchEventHandlers(has_handlers);
}

void InputRouterImpl::OnSetTouchAction(
    content::TouchAction touch_action) {
  // Synthetic touchstart events should get filtered out in RenderWidget.
  DCHECK(touch_event_queue_->IsPendingAckTouchStart());

  touch_action_filter_.OnSetTouchAction(touch_action);
}

void InputRouterImpl::ProcessInputEventAck(
    WebInputEvent::Type event_type,
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency_info,
    AckSource ack_source) {
  TRACE_EVENT2("input", "InputRouterImpl::ProcessInputEventAck",
               "type", WebInputEventTraits::GetName(event_type),
               "ack", GetEventAckName(ack_result));

  // Note: The keyboard ack must be treated carefully, as it may result in
  // synchronous destruction of |this|. Handling immediately guards against
  // future references to |this|, as with |auto_reset_current_ack_source| below.
  if (WebInputEvent::isKeyboardEventType(event_type)) {
    ProcessKeyboardAck(event_type, ack_result);
    // WARNING: |this| may be deleted at this point.
    return;
  }

  base::AutoReset<AckSource> auto_reset_current_ack_source(
      &current_ack_source_, ack_source);

  if (WebInputEvent::isMouseEventType(event_type)) {
    ProcessMouseAck(event_type, ack_result);
  } else if (event_type == WebInputEvent::MouseWheel) {
    ProcessWheelAck(ack_result, latency_info);
  } else if (WebInputEvent::isTouchEventType(event_type)) {
    ProcessTouchAck(ack_result, latency_info);
  } else if (WebInputEvent::isGestureEventType(event_type)) {
    ProcessGestureAck(event_type, ack_result, latency_info);
  } else if (event_type != WebInputEvent::Undefined) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::BAD_ACK_MESSAGE);
  }
}

void InputRouterImpl::ProcessKeyboardAck(blink::WebInputEvent::Type type,
                                         InputEventAckState ack_result) {
  if (key_queue_.empty()) {
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_ACK);
  } else if (key_queue_.front().type != type) {
    // Something must be wrong. Clear the |key_queue_| and char event
    // suppression so that we can resume from the error.
    key_queue_.clear();
    ack_handler_->OnUnexpectedEventAck(InputAckHandler::UNEXPECTED_EVENT_TYPE);
  } else {
    NativeWebKeyboardEvent front_item = key_queue_.front();
    key_queue_.pop_front();

    ack_handler_->OnKeyboardEventAck(front_item, ack_result);
    // WARNING: This InputRouterImpl can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this InputRouterImpl).
    // TODO(jdduke): crbug.com/274029 - Make ack-triggered shutdown async.
  }
}

void InputRouterImpl::ProcessMouseAck(blink::WebInputEvent::Type type,
                                      InputEventAckState ack_result) {
  if (type != WebInputEvent::MouseMove)
    return;

  mouse_move_pending_ = false;

  if (next_mouse_move_) {
    DCHECK(next_mouse_move_->event.type == WebInputEvent::MouseMove);
    scoped_ptr<MouseEventWithLatencyInfo> next_mouse_move
        = next_mouse_move_.Pass();
    SendMouseEvent(*next_mouse_move);
  }
}

void InputRouterImpl::ProcessWheelAck(InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency) {
  ProcessAckForOverscroll(current_wheel_event_.event, ack_result);

  // TODO(miletus): Add renderer side latency to each uncoalesced mouse
  // wheel event and add terminal component to each of them.
  current_wheel_event_.latency.AddNewLatencyFrom(latency);
  // Process the unhandled wheel event here before calling SendWheelEvent()
  // since it will mutate current_wheel_event_.
  ack_handler_->OnWheelEventAck(current_wheel_event_, ack_result);
  mouse_wheel_pending_ = false;

  // Now send the next (coalesced) mouse wheel event.
  if (!coalesced_mouse_wheel_events_.empty()) {
    MouseWheelEventWithLatencyInfo next_wheel_event =
        coalesced_mouse_wheel_events_.front();
    coalesced_mouse_wheel_events_.pop_front();
    SendWheelEvent(next_wheel_event);
  }
}

void InputRouterImpl::ProcessGestureAck(WebInputEvent::Type type,
                                        InputEventAckState ack_result,
                                        const ui::LatencyInfo& latency) {
  // If |ack_result| originated from the overscroll controller, only
  // feed |gesture_event_filter_| the ack if it was expecting one.
  if (current_ack_source_ == OVERSCROLL_CONTROLLER &&
      !gesture_event_filter_->HasQueuedGestureEvents()) {
    return;
  }

  // |gesture_event_filter_| will forward to OnGestureEventAck when appropriate.
  gesture_event_filter_->ProcessGestureAck(ack_result, type, latency);
}

void InputRouterImpl::ProcessTouchAck(
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency) {
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_->ProcessTouchAck(ack_result, latency);
}

void InputRouterImpl::ProcessAckForOverscroll(const WebInputEvent& event,
                                              InputEventAckState ack_result) {
  // Acks sent from the overscroll controller need not be fed back into the
  // overscroll controller.
  if (current_ack_source_ == OVERSCROLL_CONTROLLER)
    return;

  OverscrollController* controller = client_->GetOverscrollController();
  if (!controller)
    return;

  controller->ReceivedEventACK(
      event, (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result));
}

void InputRouterImpl::SimulateTouchGestureWithMouse(
    const MouseEventWithLatencyInfo& event) {
  const WebMouseEvent& mouse_event = event.event;
  int x = mouse_event.x, y = mouse_event.y;
  float dx = mouse_event.movementX, dy = mouse_event.movementY;
  static int startX = 0, startY = 0;

  switch (mouse_event.button) {
    case WebMouseEvent::ButtonLeft:
      if (mouse_event.type == WebInputEvent::MouseDown) {
        startX = x;
        startY = y;
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollBegin, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
      }
      if (dx != 0 || dy != 0) {
        GestureEventWithLatencyInfo gesture_event = MakeGestureEvent(
            WebInputEvent::GestureScrollUpdate, mouse_event.timeStampSeconds,
            x, y, 0, event.latency);
        gesture_event.event.data.scrollUpdate.deltaX = dx;
        gesture_event.event.data.scrollUpdate.deltaY = dy;
        SendGestureEvent(gesture_event);
      }
      if (mouse_event.type == WebInputEvent::MouseUp) {
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollEnd, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
      }
      break;
    case WebMouseEvent::ButtonMiddle:
      if (mouse_event.type == WebInputEvent::MouseDown) {
        startX = x;
        startY = y;
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureShowPress, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureTapDown, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
      }
      if (mouse_event.type == WebInputEvent::MouseUp) {
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureTap, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
      }
      break;
    case WebMouseEvent::ButtonRight:
      if (mouse_event.type == WebInputEvent::MouseDown) {
        startX = x;
        startY = y;
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollBegin, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GesturePinchBegin, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
      }
      if (dx != 0 || dy != 0) {
        dx = pow(dy < 0 ? 0.998f : 1.002f, fabs(dy));
        GestureEventWithLatencyInfo gesture_event = MakeGestureEvent(
            WebInputEvent::GesturePinchUpdate, mouse_event.timeStampSeconds,
            startX, startY, 0, event.latency);
        gesture_event.event.data.pinchUpdate.scale = dx;
        SendGestureEvent(gesture_event);
      }
      if (mouse_event.type == WebInputEvent::MouseUp) {
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GesturePinchEnd, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
        SendGestureEvent(MakeGestureEvent(
            WebInputEvent::GestureScrollEnd, mouse_event.timeStampSeconds,
            x, y, 0, event.latency));
      }
      break;
    case WebMouseEvent::ButtonNone:
      break;
  }
}

bool InputRouterImpl::IsInOverscrollGesture() const {
  OverscrollController* controller = client_->GetOverscrollController();
  return controller && controller->overscroll_mode() != OVERSCROLL_NONE;
}

}  // namespace content
