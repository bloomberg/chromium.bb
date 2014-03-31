// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_filter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/input/input_handler.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/vector2d_f.h"

using blink::WebInputEvent;

#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  case name::ID: return #name;

const char* GetInputMessageTypeName(const IPC::Message& message) {
  switch (message.type()) {
#include "content/common/input_messages.h"
    default:
      NOTREACHED() << "Invalid message type: " << message.type();
      break;
  };
  return "NonInputMsgType";
}

namespace content {

InputEventFilter::InputEventFilter(
    IPC::Listener* main_listener,
    const scoped_refptr<base::MessageLoopProxy>& target_loop)
    : main_loop_(base::MessageLoopProxy::current()),
      main_listener_(main_listener),
      sender_(NULL),
      target_loop_(target_loop),
      overscroll_notifications_enabled_(false) {
  DCHECK(target_loop_.get());
  overscroll_notifications_enabled_ =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOverscrollNotifications);
}

void InputEventFilter::SetBoundHandler(const Handler& handler) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  handler_ = handler;
}

void InputEventFilter::DidAddInputHandler(int routing_id,
                                          cc::InputHandler* input_handler) {
  base::AutoLock locked(routes_lock_);
  routes_.insert(routing_id);
}

void InputEventFilter::DidRemoveInputHandler(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.erase(routing_id);
}

void InputEventFilter::DidOverscroll(int routing_id,
                                     const DidOverscrollParams& params) {
  if (!overscroll_notifications_enabled_)
    return;

  // TODO(jdduke): Use |params| as the sole message argument, crbug/354444.
  SendMessage(ViewHostMsg_DidOverscroll(routing_id,
                                        params.accumulated_overscroll,
                                        params.current_fling_velocity));
}

void InputEventFilter::DidStopFlinging(int routing_id) {
  SendMessage(ViewHostMsg_DidStopFlinging(routing_id));
}

void InputEventFilter::OnFilterAdded(IPC::Channel* channel) {
  io_loop_ = base::MessageLoopProxy::current();
  sender_ = channel;
}

void InputEventFilter::OnFilterRemoved() {
  sender_ = NULL;
}

void InputEventFilter::OnChannelClosing() {
  sender_ = NULL;
}

// This function returns true if the IPC message is one that the compositor
// thread can handle *or* one that needs to preserve relative order with
// messages that the compositor thread can handle. Returning true for a message
// type means that the message will go through an extra copy and thread hop, so
// use with care.
static bool RequiresThreadBounce(const IPC::Message& message) {
  return IPC_MESSAGE_ID_CLASS(message.type()) == InputMsgStart;
}

bool InputEventFilter::OnMessageReceived(const IPC::Message& message) {
  if (!RequiresThreadBounce(message))
    return false;

  TRACE_EVENT0("input", "InputEventFilter::OnMessageReceived::InputMessage");

  {
    base::AutoLock locked(routes_lock_);
    if (routes_.find(message.routing_id()) == routes_.end())
      return false;
  }

  target_loop_->PostTask(
      FROM_HERE,
      base::Bind(&InputEventFilter::ForwardToHandler, this, message));
  return true;
}

InputEventFilter::~InputEventFilter() {
}

void InputEventFilter::ForwardToMainListener(const IPC::Message& message) {
  main_listener_->OnMessageReceived(message);
}

void InputEventFilter::ForwardToHandler(const IPC::Message& message) {
  DCHECK(!handler_.is_null());
  DCHECK(target_loop_->BelongsToCurrentThread());
  TRACE_EVENT1("input", "InputEventFilter::ForwardToHandler",
               "message_type", GetInputMessageTypeName(message));

  if (message.type() != InputMsg_HandleInputEvent::ID) {
    TRACE_EVENT_INSTANT0(
        "input",
        "InputEventFilter::ForwardToHandler::ForwardToMainListener",
        TRACE_EVENT_SCOPE_THREAD);
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&InputEventFilter::ForwardToMainListener,
                   this, message));
    return;
  }

  int routing_id = message.routing_id();
  ui::LatencyInfo latency_info;
  const WebInputEvent* event = NULL;
  bool is_keyboard_shortcut;
  if (!InputMsg_HandleInputEvent::Read(
          &message, &event, &latency_info, &is_keyboard_shortcut))
    return;
  DCHECK(event);

  InputEventAckState ack = handler_.Run(routing_id, event, &latency_info);

  if (ack == INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    TRACE_EVENT_INSTANT0(
        "input",
        "InputEventFilter::ForwardToHandler::ForwardToMainListener",
        TRACE_EVENT_SCOPE_THREAD);
    IPC::Message new_msg = InputMsg_HandleInputEvent(
        routing_id, event, latency_info, is_keyboard_shortcut);
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&InputEventFilter::ForwardToMainListener,
                   this, new_msg));
    return;
  }

  if (!WebInputEventTraits::IgnoresAckDisposition(event->type))
    SendACK(event->type, ack, latency_info, routing_id);
}

void InputEventFilter::SendACK(blink::WebInputEvent::Type type,
                               InputEventAckState ack_result,
                               const ui::LatencyInfo& latency_info,
                               int routing_id) {
  SendMessage(InputHostMsg_HandleInputEvent_ACK(
      routing_id, type, ack_result, latency_info));
}

void InputEventFilter::SendMessage(const IPC::Message& message) {
  DCHECK(target_loop_->BelongsToCurrentThread());

  io_loop_->PostTask(
      FROM_HERE,
      base::Bind(&InputEventFilter::SendMessageOnIOThread, this, message));
}

void InputEventFilter::SendMessageOnIOThread(const IPC::Message& message) {
  DCHECK(io_loop_->BelongsToCurrentThread());

  if (!sender_)
    return;  // Filter was removed.

  sender_->Send(new IPC::Message(message));
}

}  // namespace content
