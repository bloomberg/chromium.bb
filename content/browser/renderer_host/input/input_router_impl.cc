// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/web_touch_event_traits.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/touch_action.h"
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

#if defined(OS_ANDROID)
#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/screen.h"
#else
#include "ui/events/gestures/gesture_configuration.h"
#endif

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

// TODO(jdduke): Instead of relying on command line flags or conditional
// conditional compilation here, we should instead use an InputRouter::Settings
// construct, supplied and customized by the RenderWidgetHostView. See
// crbug.com/343917.
bool GetTouchAckTimeoutDelay(base::TimeDelta* touch_ack_timeout_delay) {
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();
  if (!parsed_command_line->HasSwitch(switches::kTouchAckTimeoutDelayMs))
    return false;

  std::string timeout_string = parsed_command_line->GetSwitchValueASCII(
      switches::kTouchAckTimeoutDelayMs);
  size_t timeout_ms;
  if (!base::StringToSizeT(timeout_string, &timeout_ms))
    return false;

  *touch_ack_timeout_delay = base::TimeDelta::FromMilliseconds(timeout_ms);
  return true;
}

#if defined(OS_ANDROID)
double GetTouchMoveSlopSuppressionLengthDips() {
  const double touch_slop_length_pixels =
      static_cast<double>(gfx::ViewConfiguration::GetTouchSlopInPixels());
  const double device_scale_factor =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().device_scale_factor();
  return touch_slop_length_pixels / device_scale_factor;
}
#elif defined(USE_AURA)
double GetTouchMoveSlopSuppressionLengthDips() {
  return ui::GestureConfiguration::max_touch_move_in_pixels_for_click();
}
#else
double GetTouchMoveSlopSuppressionLengthDips() {
  return 0;
}
#endif

TouchEventQueue::TouchScrollingMode GetTouchScrollingMode() {
  std::string modeString = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kTouchScrollingMode);
  if (modeString == switches::kTouchScrollingModeSyncTouchmove)
    return TouchEventQueue::TOUCH_SCROLLING_MODE_SYNC_TOUCHMOVE;
  if (modeString == switches::kTouchScrollingModeAbsorbTouchmove)
    return TouchEventQueue::TOUCH_SCROLLING_MODE_ABSORB_TOUCHMOVE;
  if (modeString == switches::kTouchScrollingModeTouchcancel)
    return TouchEventQueue::TOUCH_SCROLLING_MODE_TOUCHCANCEL;
  if (modeString != "")
    LOG(ERROR) << "Invalid --touch-scrolling-mode option: " << modeString;
  return TouchEventQueue::TOUCH_SCROLLING_MODE_DEFAULT;
}

GestureEventWithLatencyInfo MakeGestureEvent(WebInputEvent::Type type,
                                             double timestamp_seconds,
                                             int x,
                                             int y,
                                             int modifiers,
                                             const ui::LatencyInfo& latency) {
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
      touch_ack_timeout_supported_(false),
      current_view_flags_(0),
      current_ack_source_(ACK_SOURCE_NONE),
      flush_requested_(false),
      touch_event_queue_(this,
                         GetTouchScrollingMode(),
                         GetTouchMoveSlopSuppressionLengthDips()),
      gesture_event_queue_(this, this) {
  DCHECK(sender);
  DCHECK(client);
  DCHECK(ack_handler);
  touch_ack_timeout_supported_ =
      GetTouchAckTimeoutDelay(&touch_ack_timeout_delay_);
  UpdateTouchAckTimeoutEnabled();
}

InputRouterImpl::~InputRouterImpl() {}

void InputRouterImpl::Flush() {
  flush_requested_ = true;
  SignalFlushedIfNecessary();
}

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
      gesture_event_queue_.GetTouchpadTapSuppressionController()->
          ShouldDeferMouseDown(mouse_event))
      return;
  if (mouse_event.event.type == WebInputEvent::MouseUp &&
      gesture_event_queue_.GetTouchpadTapSuppressionController()->
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

  gesture_event_queue_.FlingHasBeenHalted();

  // Only forward the non-native portions of our event.
  FilterAndSendWebInputEvent(key_event, latency_info, is_keyboard_shortcut);
}

void InputRouterImpl::SendGestureEvent(
    const GestureEventWithLatencyInfo& original_gesture_event) {
  event_stream_validator_.OnEvent(original_gesture_event.event);
  GestureEventWithLatencyInfo gesture_event(original_gesture_event);

  if (touch_action_filter_.FilterGestureEvent(&gesture_event.event))
    return;

  touch_event_queue_.OnGestureScrollEvent(gesture_event);

  if (!IsInOverscrollGesture() &&
      !gesture_event_queue_.ShouldForward(gesture_event)) {
    OverscrollController* controller = client_->GetOverscrollController();
    if (controller)
      controller->DiscardingGestureEvent(gesture_event.event);
    return;
  }

  FilterAndSendWebInputEvent(gesture_event.event, gesture_event.latency, false);
}

void InputRouterImpl::SendTouchEvent(
    const TouchEventWithLatencyInfo& touch_event) {
  touch_event_queue_.QueueEvent(touch_event);
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
  if (WebTouchEventTraits::IsTouchSequenceStart(touch_event.event)) {
    touch_action_filter_.ResetTouchAction();
    // Note that if the previous touch-action was TOUCH_ACTION_NONE, enabling
    // the timeout here will not take effect until the *following* touch
    // sequence.  This is a desirable side-effect, giving the renderer a chance
    // to send a touch-action response without racing against the ack timeout.
    UpdateTouchAckTimeoutEnabled();
  }

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
  // Always send a touch event if the renderer has a touch-event handler or
  // there are pending touch events.
  return touch_event_queue_.has_handlers() || !touch_event_queue_.empty();
}

void InputRouterImpl::OnViewUpdated(int view_flags) {
  current_view_flags_ = view_flags;

  // A fixed page scale or mobile viewport should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
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
  // Touchstart events sent to the renderer indicate a new touch sequence, but
  // in some cases we may filter out sending the touchstart - catch those here.
  if (WebTouchEventTraits::IsTouchSequenceStart(event.event) &&
      ack_result == INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS) {
    touch_action_filter_.ResetTouchAction();
    UpdateTouchAckTimeoutEnabled();
  }
  ack_handler_->OnTouchEventAck(event, ack_result);
}

void InputRouterImpl::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  touch_event_queue_.OnGestureEventAck(event, ack_result);
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
  // Trackpad pinch gestures are not yet handled by the renderer.
  // TODO(rbyers): Send mousewheel for trackpad pinch - crbug.com/289887.
  if (input_event.type == WebInputEvent::GesturePinchUpdate &&
      static_cast<const WebGestureEvent&>(input_event).sourceDevice ==
          WebGestureEvent::Touchpad) {
    ProcessInputEventAck(input_event.type,
                         INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                         latency_info,
                         ACK_SOURCE_NONE);
    return;
  }

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

  if (disposition == OverscrollController::SHOULD_FORWARD_TO_GESTURE_QUEUE) {
    DCHECK(WebInputEvent::isGestureEventType(input_event.type));
    const blink::WebGestureEvent& gesture_event =
        static_cast<const blink::WebGestureEvent&>(input_event);
    // An ACK is expected for the event, so mark it as consumed.
    consumed = !gesture_event_queue_.ShouldForward(
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
  if (Send(new InputMsg_HandleInputEvent(
          routing_id(), &input_event, latency_info, is_keyboard_shortcut))) {
    // Ack messages for ignored ack event types are not required, and might
    // never be sent by the renderer. Consequently, such event types should not
    // affect event timing or in-flight event count metrics.
    if (!WebInputEventTraits::IgnoresAckDisposition(input_event.type)) {
      input_event_start_time_ = TimeTicks::Now();
      client_->IncrementInFlightEventCount();
    }
    return true;
  }
  return false;
}

void InputRouterImpl::OnInputEventAck(WebInputEvent::Type event_type,
                                      InputEventAckState ack_result,
                                      const ui::LatencyInfo& latency_info) {
  // A synthetic ack will already have been sent for this event, and it should
  // not affect event timing or in-flight count metrics.
  if (WebInputEventTraits::IgnoresAckDisposition(event_type))
    return;

  client_->DecrementInFlightEventCount();

  // Log the time delta for processing an input event.
  TimeDelta delta = TimeTicks::Now() - input_event_start_time_;
  UMA_HISTOGRAM_TIMES("MPArch.IIR_InputEventDelta", delta);

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
  TRACE_EVENT1("input", "InputRouterImpl::OnHasTouchEventHandlers",
               "has_handlers", has_handlers);

  touch_event_queue_.OnHasTouchEventHandlers(has_handlers);
  client_->OnHasTouchEventHandlers(has_handlers);
}

void InputRouterImpl::OnSetTouchAction(TouchAction touch_action) {
  // Synthetic touchstart events should get filtered out in RenderWidget.
  DCHECK(touch_event_queue_.IsPendingAckTouchStart());
  TRACE_EVENT1("input", "InputRouterImpl::OnSetTouchAction",
               "action", touch_action);

  touch_action_filter_.OnSetTouchAction(touch_action);

  // TOUCH_ACTION_NONE should disable the touch ack timeout.
  UpdateTouchAckTimeoutEnabled();
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

  SignalFlushedIfNecessary();
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
  // feed |gesture_event_queue_| the ack if it was expecting one.
  if (current_ack_source_ == OVERSCROLL_CONTROLLER &&
      !gesture_event_queue_.ExpectingGestureAck()) {
    return;
  }

  // |gesture_event_queue_| will forward to OnGestureEventAck when appropriate.
  gesture_event_queue_.ProcessGestureAck(ack_result, type, latency);
}

void InputRouterImpl::ProcessTouchAck(
    InputEventAckState ack_result,
    const ui::LatencyInfo& latency) {
  // |touch_event_queue_| will forward to OnTouchEventAck when appropriate.
  touch_event_queue_.ProcessTouchAck(ack_result, latency);
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

void InputRouterImpl::UpdateTouchAckTimeoutEnabled() {
  if (!touch_ack_timeout_supported_) {
    touch_event_queue_.SetAckTimeoutEnabled(false, base::TimeDelta());
    return;
  }

  // Mobile sites tend to be well-behaved with respect to touch handling, so
  // they have less need for the touch timeout fallback.
  const bool fixed_page_scale = (current_view_flags_ & FIXED_PAGE_SCALE) != 0;
  const bool mobile_viewport = (current_view_flags_ & MOBILE_VIEWPORT) != 0;

  // TOUCH_ACTION_NONE will prevent scrolling, in which case the timeout serves
  // little purpose. It's also a strong signal that touch handling is critical
  // to page functionality, so the timeout could do more harm than good.
  const bool touch_action_none =
      touch_action_filter_.allowed_touch_action() == TOUCH_ACTION_NONE;

  const bool touch_ack_timeout_enabled = !fixed_page_scale &&
                                         !mobile_viewport &&
                                         !touch_action_none;
  touch_event_queue_.SetAckTimeoutEnabled(touch_ack_timeout_enabled,
                                          touch_ack_timeout_delay_);
}

void InputRouterImpl::SignalFlushedIfNecessary() {
  if (!flush_requested_)
    return;

  if (HasPendingEvents())
    return;

  flush_requested_ = false;
  client_->DidFlush();
}

bool InputRouterImpl::HasPendingEvents() const {
  return !touch_event_queue_.empty() ||
         !gesture_event_queue_.empty() ||
         !key_queue_.empty() ||
         mouse_move_pending_ ||
         mouse_wheel_pending_ ||
         select_range_pending_ ||
         move_caret_pending_;
}

bool InputRouterImpl::IsInOverscrollGesture() const {
  OverscrollController* controller = client_->GetOverscrollController();
  return controller && controller->overscroll_mode() != OVERSCROLL_NONE;
}

}  // namespace content
