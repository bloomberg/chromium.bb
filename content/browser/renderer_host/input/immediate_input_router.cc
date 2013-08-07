// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/immediate_input_router.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "content/browser/renderer_host/input/gesture_event_filter.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

namespace content {
namespace {

// Returns |true| if the two wheel events should be coalesced.
bool ShouldCoalesceMouseWheelEvents(const WebMouseWheelEvent& last_event,
                                    const WebMouseWheelEvent& new_event) {
  return last_event.modifiers == new_event.modifiers &&
         last_event.scrollByPage == new_event.scrollByPage &&
         last_event.hasPreciseScrollingDeltas
             == new_event.hasPreciseScrollingDeltas &&
         last_event.phase == new_event.phase &&
         last_event.momentumPhase == new_event.momentumPhase;
}

float GetUnacceleratedDelta(float accelerated_delta, float acceleration_ratio) {
  return accelerated_delta * acceleration_ratio;
}

float GetAccelerationRatio(float accelerated_delta, float unaccelerated_delta) {
  if (unaccelerated_delta == 0.f || accelerated_delta == 0.f)
    return 1.f;
  return unaccelerated_delta / accelerated_delta;
}

const char* GetEventAckName(InputEventAckState ack_result) {
  switch(ack_result) {
    case INPUT_EVENT_ACK_STATE_UNKNOWN: return "UNKNOWN";
    case INPUT_EVENT_ACK_STATE_CONSUMED: return "CONSUMED";
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED: return "NOT_CONSUMED";
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS: return "NO_CONSUMER_EXISTS";
  default:
    DLOG(WARNING) << "Unhandled InputEventAckState in GetEventAckName.\n";
    break;
  }
  return "";
}

} // namespace

ImmediateInputRouter::ImmediateInputRouter(
    RenderProcessHost* process,
    InputRouterClient* client,
    int routing_id)
    : process_(process),
      client_(client),
      routing_id_(routing_id),
      select_range_pending_(false),
      move_caret_pending_(false),
      mouse_move_pending_(false),
      mouse_wheel_pending_(false),
      has_touch_handler_(false),
      touch_event_queue_(new TouchEventQueue(this)),
      gesture_event_filter_(new GestureEventFilter(this)) {
  DCHECK(process);
  DCHECK(client);
}

ImmediateInputRouter::~ImmediateInputRouter() {
}

bool ImmediateInputRouter::SendInput(IPC::Message* message) {
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) == InputMsgStart);
  scoped_ptr<IPC::Message> scoped_message(message);
  switch (scoped_message->type()) {
    // Check for types that require an ACK.
    case InputMsg_SelectRange::ID:
      return SendSelectRange(scoped_message.release());
    case InputMsg_MoveCaret::ID:
      return SendMoveCaret(scoped_message.release());
    case InputMsg_HandleInputEvent::ID:
      NOTREACHED() << "WebInputEvents should never be sent via SendInput.";
      return false;
    default:
      return Send(scoped_message.release());
  }
}

void ImmediateInputRouter::SendMouseEvent(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (!client_->OnSendMouseEvent(mouse_event))
    return;

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

void ImmediateInputRouter::SendWheelEvent(
    const MouseWheelEventWithLatencyInfo& wheel_event) {
  if (!client_->OnSendWheelEvent(wheel_event))
    return;

  // If there's already a mouse wheel event waiting to be sent to the renderer,
  // add the new deltas to that event. Not doing so (e.g., by dropping the old
  // event, as for mouse moves) results in very slow scrolling on the Mac (on
  // which many, very small wheel events are sent).
  if (mouse_wheel_pending_) {
    if (coalesced_mouse_wheel_events_.empty() ||
        !ShouldCoalesceMouseWheelEvents(
            coalesced_mouse_wheel_events_.back().event, wheel_event.event)) {
      coalesced_mouse_wheel_events_.push_back(wheel_event);
    } else {
      MouseWheelEventWithLatencyInfo* last_wheel_event =
          &coalesced_mouse_wheel_events_.back();
      float unaccelerated_x =
          GetUnacceleratedDelta(last_wheel_event->event.deltaX,
                                last_wheel_event->event.accelerationRatioX) +
          GetUnacceleratedDelta(wheel_event.event.deltaX,
                                wheel_event.event.accelerationRatioX);
      float unaccelerated_y =
          GetUnacceleratedDelta(last_wheel_event->event.deltaY,
                                last_wheel_event->event.accelerationRatioY) +
          GetUnacceleratedDelta(wheel_event.event.deltaY,
                                wheel_event.event.accelerationRatioY);
      last_wheel_event->event.deltaX += wheel_event.event.deltaX;
      last_wheel_event->event.deltaY += wheel_event.event.deltaY;
      last_wheel_event->event.wheelTicksX += wheel_event.event.wheelTicksX;
      last_wheel_event->event.wheelTicksY += wheel_event.event.wheelTicksY;
      last_wheel_event->event.accelerationRatioX =
          GetAccelerationRatio(last_wheel_event->event.deltaX, unaccelerated_x);
      last_wheel_event->event.accelerationRatioY =
          GetAccelerationRatio(last_wheel_event->event.deltaY, unaccelerated_y);
      DCHECK_GE(wheel_event.event.timeStampSeconds,
                last_wheel_event->event.timeStampSeconds);
      last_wheel_event->event.timeStampSeconds =
          wheel_event.event.timeStampSeconds;
      last_wheel_event->latency.MergeWith(wheel_event.latency);
    }
    return;
  }
  mouse_wheel_pending_ = true;
  current_wheel_event_ = wheel_event;

  HISTOGRAM_COUNTS_100("Renderer.WheelQueueSize",
                       coalesced_mouse_wheel_events_.size());

  FilterAndSendWebInputEvent(wheel_event.event, wheel_event.latency, false);
}

void ImmediateInputRouter::SendKeyboardEvent(
    const NativeWebKeyboardEvent& key_event,
    const ui::LatencyInfo& latency_info) {
  bool is_shortcut = false;
  if (!client_->OnSendKeyboardEvent(key_event, latency_info, &is_shortcut))
    return;

  // Put all WebKeyboardEvent objects in a queue since we can't trust the
  // renderer and we need to give something to the HandleKeyboardEvent
  // handler.
  key_queue_.push_back(key_event);
  HISTOGRAM_COUNTS_100("Renderer.KeyboardQueueSize", key_queue_.size());

  gesture_event_filter_->FlingHasBeenHalted();

  // Only forward the non-native portions of our event.
  FilterAndSendWebInputEvent(key_event, latency_info, is_shortcut);
}

void ImmediateInputRouter::SendGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!client_->OnSendGestureEvent(gesture_event))
    return;
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency, false);
}

void ImmediateInputRouter::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  // Always queue TouchEvents, even if the client request they be dropped.
  client_->OnSendTouchEvent(touch_event);
  touch_event_queue_->QueueEvent(touch_event);
}

// Forwards MouseEvent without passing it through
// TouchpadTapSuppressionController.
void ImmediateInputRouter::SendMouseEventImmediately(
    const MouseEventWithLatencyInfo& mouse_event) {
  if (!client_->OnSendMouseEventImmediately(mouse_event))
    return;

  // Avoid spamming the renderer with mouse move events.  It is important
  // to note that WM_MOUSEMOVE events are anyways synthetic, but since our
  // thread is able to rapidly consume WM_MOUSEMOVE events, we may get way
  // more WM_MOUSEMOVE events than we wish to send to the renderer.
  if (mouse_event.event.type == WebInputEvent::MouseMove) {
    if (mouse_move_pending_) {
      if (!next_mouse_move_) {
        next_mouse_move_.reset(new MouseEventWithLatencyInfo(mouse_event));
      } else {
        // Accumulate movement deltas.
        int x = next_mouse_move_->event.movementX;
        int y = next_mouse_move_->event.movementY;
        next_mouse_move_->event = mouse_event.event;
        next_mouse_move_->event.movementX += x;
        next_mouse_move_->event.movementY += y;
        next_mouse_move_->latency.MergeWith(mouse_event.latency);
      }
      return;
    }
    mouse_move_pending_ = true;
  }

  FilterAndSendWebInputEvent(mouse_event.event, mouse_event.latency, false);
}

void ImmediateInputRouter::SendTouchEventImmediately(
    const TouchEventWithLatencyInfo& touch_event) {
  if (!client_->OnSendTouchEventImmediately(touch_event))
    return;
  FilterAndSendWebInputEvent(touch_event.event, touch_event.latency, false);
}

void ImmediateInputRouter::SendGestureEventImmediately(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (!client_->OnSendGestureEventImmediately(gesture_event))
    return;
  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency, false);
}

const NativeWebKeyboardEvent*
    ImmediateInputRouter::GetLastKeyboardEvent() const {
  if (key_queue_.empty())
    return NULL;
  return &key_queue_.front();
}

bool ImmediateInputRouter::ShouldForwardTouchEvent() const {
  // Always send a touch event if the renderer has a touch-event handler. It is
  // possible that a renderer stops listening to touch-events while there are
  // still events in the touch-queue. In such cases, the new events should still
  // get into the queue.
  return has_touch_handler_ || !touch_event_queue_->empty();
}

bool ImmediateInputRouter::ShouldForwardGestureEvent(
    const GestureEventWithLatencyInfo& touch_event) const {
  return gesture_event_filter_->ShouldForward(touch_event);
}

bool ImmediateInputRouter::HasQueuedGestureEvents() const {
  return gesture_event_filter_->HasQueuedGestureEvents();
}

bool ImmediateInputRouter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ImmediateInputRouter, message, message_is_ok)
    IPC_MESSAGE_HANDLER(InputHostMsg_HandleInputEvent_ACK, OnInputEventAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MoveCaret_ACK, OnMsgMoveCaretAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SelectRange_ACK, OnSelectRangeAck)
    IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
                        OnHasTouchEventHandlers)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!message_is_ok)
    client_->OnUnexpectedEventAck(true);

  return handled;
}

void ImmediateInputRouter::OnTouchEventAck(
    const TouchEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  client_->OnTouchEventAck(event, ack_result);
}

bool ImmediateInputRouter::SendSelectRange(IPC::Message* message) {
  DCHECK(message->type() == InputMsg_SelectRange::ID);
  if (select_range_pending_) {
    next_selection_range_.reset(message);
    return true;
  }

  select_range_pending_ = true;
  return Send(message);
}

bool ImmediateInputRouter::SendMoveCaret(IPC::Message* message) {
  DCHECK(message->type() == InputMsg_MoveCaret::ID);
  if (move_caret_pending_) {
    next_move_caret_.reset(message);
    return true;
  }

  move_caret_pending_ = true;
  return Send(message);
}

bool ImmediateInputRouter::Send(IPC::Message* message) {
  return process_->Send(message);
}

void ImmediateInputRouter::SendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    bool is_keyboard_shortcut) {
  input_event_start_time_ = TimeTicks::Now();
  Send(new InputMsg_HandleInputEvent(
      routing_id(), &input_event, latency_info, is_keyboard_shortcut));
  client_->IncrementInFlightEventCount();
}

void ImmediateInputRouter::FilterAndSendWebInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    bool is_keyboard_shortcut) {
  TRACE_EVENT0("input", "ImmediateInputRouter::FilterAndSendWebInputEvent");

  if (!process_->HasConnection())
    return;

  DCHECK(!process_->IgnoreInputEvents());

  // Perform optional, synchronous event handling, sending ACK messages for
  // processed events, or proceeding as usual.
  InputEventAckState filter_ack = client_->FilterInputEvent(input_event,
                                                            latency_info);
  switch (filter_ack) {
    // Send the ACK and early exit.
    case INPUT_EVENT_ACK_STATE_CONSUMED:
    case INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS:
      next_mouse_move_.reset();
      ProcessInputEventAck(input_event.type, filter_ack, latency_info);
      // WARNING: |this| may be deleted at this point.
      return;

    case INPUT_EVENT_ACK_STATE_UNKNOWN: {
      if (input_event.type == WebKit::WebInputEvent::MouseMove) {
        // Since this mouse-move event has been consumed, there will be no ACKs.
        // So reset the state here so that future mouse-move events do reach the
        // renderer.
        mouse_move_pending_ = false;
      } else if (input_event.type == WebKit::WebInputEvent::MouseWheel) {
        // Reset the wheel-event state when appropriate.
        mouse_wheel_pending_ = false;
      } else if (WebInputEvent::isGestureEventType(input_event.type) &&
                 gesture_event_filter_->HasQueuedGestureEvents()) {
        // If the gesture-event filter has queued gesture events, that implies
        // it's awaiting an ack for the event. Since the event is being dropped,
        // it is never sent to the renderer, and so it won't receive any ACKs.
        // So send the ACK to the gesture event filter immediately, and mark it
        // as having been processed.
        gesture_event_filter_->ProcessGestureAck(true, input_event.type);
      } else if (WebInputEvent::isTouchEventType(input_event.type)) {
        // During an overscroll gesture initiated by touch-scrolling, the
        // touch-events do not reset or contribute to the overscroll gesture.
        // However, the touch-events are not sent to the renderer. So send an
        // ACK to the touch-event queue immediately. Mark the event as not
        // processed, to make sure that the touch-scroll gesture that initiated
        // the overscroll is updated properly.
        touch_event_queue_->ProcessTouchAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                                            latency_info);
      }
      return;
    }

    // Proceed as normal.
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
      break;
  };

  // Transmit any pending wheel events on a non-wheel event. This ensures that
  // the renderer receives the final PhaseEnded wheel event, which is necessary
  // to terminate rubber-banding, for example.
  if (input_event.type != WebInputEvent::MouseWheel) {
    for (size_t i = 0; i < coalesced_mouse_wheel_events_.size(); ++i) {
      SendWebInputEvent(coalesced_mouse_wheel_events_[i].event,
                       coalesced_mouse_wheel_events_[i].latency,
                      false);
    }
    coalesced_mouse_wheel_events_.clear();
  }

  SendWebInputEvent(input_event, latency_info, is_keyboard_shortcut);

  // Any input event cancels a pending mouse move event.
  next_mouse_move_.reset();
}

void ImmediateInputRouter::OnInputEventAck(
    WebInputEvent::Type event_type,
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency_info) {
  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.IIR_InputEventDelta", delta);

  client_->DecrementInFlightEventCount();

  ProcessInputEventAck(event_type, ack_result, latency_info);
}

void ImmediateInputRouter::OnMsgMoveCaretAck() {
  move_caret_pending_ = false;
  if (next_move_caret_)
    SendMoveCaret(next_move_caret_.release());
}

void ImmediateInputRouter::OnSelectRangeAck() {
  select_range_pending_ = false;
  if (next_selection_range_)
    SendSelectRange(next_selection_range_.release());
}

void ImmediateInputRouter::OnHasTouchEventHandlers(bool has_handlers) {
 if (has_touch_handler_ == has_handlers)
    return;
  has_touch_handler_ = has_handlers;
  if (!has_handlers)
    touch_event_queue_->FlushQueue();
  client_->OnHasTouchEventHandlers(has_handlers);
}

void ImmediateInputRouter::ProcessInputEventAck(
    WebInputEvent::Type event_type,
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency_info) {
  TRACE_EVENT1("input", "ImmediateInputRouter::ProcessInputEventAck",
               "ack", GetEventAckName(ack_result));

  int type = static_cast<int>(event_type);
  if (type < WebInputEvent::Undefined) {
    client_->OnUnexpectedEventAck(true);
  } else if (type == WebInputEvent::MouseMove) {
    mouse_move_pending_ = false;

    // now, we can send the next mouse move event
    if (next_mouse_move_) {
      DCHECK(next_mouse_move_->event.type == WebInputEvent::MouseMove);
      scoped_ptr<MouseEventWithLatencyInfo> next_mouse_move
          = next_mouse_move_.Pass();
      SendMouseEvent(*next_mouse_move);
    }
  } else if (WebInputEvent::isKeyboardEventType(type)) {
    ProcessKeyboardAck(type, ack_result);
  } else if (type == WebInputEvent::MouseWheel) {
    ProcessWheelAck(ack_result);
  } else if (WebInputEvent::isTouchEventType(type)) {
    ProcessTouchAck(ack_result, latency_info);
  } else if (WebInputEvent::isGestureEventType(type)) {
    ProcessGestureAck(type, ack_result);
  }

  // WARNING: |this| may be deleted at this point.

  // This is used only for testing, and the other end does not use the
  // source object.  On linux, specifying
  // Source<RenderWidgetHost> results in a very strange
  // runtime error in the epilogue of the enclosing
  // (ProcessInputEventAck) method, but not on other platforms; using
  // 'void' instead is just as safe (since NotificationSource
  // is not actually typesafe) and avoids this error.
  NotificationService::current()->Notify(
      NOTIFICATION_RENDER_WIDGET_HOST_DID_RECEIVE_INPUT_EVENT_ACK,
      Source<void>(this),
      Details<int>(&type));
}

void ImmediateInputRouter::ProcessKeyboardAck(
    int type,
    InputEventAckState ack_result) {
  if (key_queue_.empty()) {
    LOG(ERROR) << "Got a KeyEvent back from the renderer but we "
               << "don't seem to have sent it to the renderer!";
  } else if (key_queue_.front().type != type) {
    LOG(ERROR) << "We seem to have a different key type sent from "
               << "the renderer. (" << key_queue_.front().type << " vs. "
               << type << "). Ignoring event.";

    // Something must be wrong. Clear the |key_queue_| and char event
    // suppression so that we can resume from the error.
    key_queue_.clear();
    client_->OnUnexpectedEventAck(false);
  } else {
    NativeWebKeyboardEvent front_item = key_queue_.front();
    key_queue_.pop_front();

    client_->OnKeyboardEventAck(front_item, ack_result);

    // WARNING: This ImmediateInputRouter can be deallocated at this point
    // (i.e.  in the case of Ctrl+W, where the call to
    // HandleKeyboardEvent destroys this ImmediateInputRouter).
  }
}

void ImmediateInputRouter::ProcessWheelAck(InputEventAckState ack_result) {
  mouse_wheel_pending_ = false;

  // Process the unhandled wheel event here before calling
  // ForwardWheelEventWithLatencyInfo() since it will mutate
  // current_wheel_event_.
  client_->OnWheelEventAck(current_wheel_event_.event, ack_result);

  // Now send the next (coalesced) mouse wheel event.
  if (!coalesced_mouse_wheel_events_.empty()) {
    MouseWheelEventWithLatencyInfo next_wheel_event =
        coalesced_mouse_wheel_events_.front();
    coalesced_mouse_wheel_events_.pop_front();
    SendWheelEvent(next_wheel_event);
  }
}

void ImmediateInputRouter::ProcessGestureAck(int type,
                                             InputEventAckState ack_result) {
  const bool processed = (INPUT_EVENT_ACK_STATE_CONSUMED == ack_result);
  client_->OnGestureEventAck(
      gesture_event_filter_->GetGestureEventAwaitingAck(), ack_result);
  gesture_event_filter_->ProcessGestureAck(processed, type);
}

void ImmediateInputRouter::ProcessTouchAck(
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency_info) {
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_->ProcessTouchAck(ack_result, latency_info);
}

}  // namespace content
