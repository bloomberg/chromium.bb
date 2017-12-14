// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_filter.h"

#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/vector2d_f.h"

using blink::WebInputEvent;
using ui::DidOverscrollParams;

const char* GetInputMessageTypeName(const IPC::Message& message);

namespace content {

InputEventFilter::InputEventFilter(
    const base::Callback<void(const IPC::Message&)>& main_listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner)
    : main_task_runner_(main_task_runner),
      main_listener_(main_listener),
      sender_(nullptr),
      target_task_runner_(target_task_runner),
      input_handler_manager_(nullptr) {
  DCHECK(target_task_runner_.get());
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void InputEventFilter::SetInputHandlerManager(
    InputHandlerManager* input_handler_manager) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  input_handler_manager_ = input_handler_manager;
}

void InputEventFilter::RegisterRoutingID(
    int routing_id,
    const scoped_refptr<MainThreadEventQueue>& input_event_queue) {
  base::AutoLock locked(routes_lock_);
  routes_.insert(routing_id);
  route_queues_[routing_id] = input_event_queue;
}

void InputEventFilter::RegisterAssociatedRenderFrameRoutingID(
    int render_frame_routing_id,
    int render_view_routing_id) {
  base::AutoLock locked(routes_lock_);
  DCHECK(routes_.find(render_view_routing_id) != routes_.end());
  associated_routes_[render_frame_routing_id] = render_view_routing_id;
}

void InputEventFilter::UnregisterRoutingID(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.erase(routing_id);
  route_queues_.erase(routing_id);
  associated_routes_.erase(routing_id);
}

void InputEventFilter::DidOverscroll(int routing_id,
                                     const DidOverscrollParams& params) {
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_DidOverscroll(routing_id, params)));
}

void InputEventFilter::DidStopFlinging(int routing_id) {
  SendMessage(std::make_unique<InputHostMsg_DidStopFlinging>(routing_id));
}

void InputEventFilter::QueueClosureForMainThreadEventQueue(
    int routing_id,
    const base::Closure& closure) {
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  RouteQueueMap::iterator iter = route_queues_.find(routing_id);
  if (iter != route_queues_.end()) {
    iter->second->QueueClosure(closure);
    return;
  }

  // For some reason we didn't find an event queue for the route.
  // Don't drop the task on the floor allow it to execute.
  main_task_runner_->PostTask(FROM_HERE, closure);
}

void InputEventFilter::DispatchNonBlockingEventToMainThread(
    int routing_id,
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info) {
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  RouteQueueMap::iterator iter = route_queues_.find(routing_id);
  if (iter != route_queues_.end()) {
    iter->second->HandleEvent(
        std::move(event), latency_info, DISPATCH_TYPE_NON_BLOCKING,
        INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING, HandledEventCallback());
  }
}

void InputEventFilter::SetWhiteListedTouchAction(int routing_id,
                                                 cc::TouchAction touch_action,
                                                 uint32_t unique_touch_event_id,
                                                 InputEventAckState ack_state) {
  SendMessage(std::make_unique<InputHostMsg_SetWhiteListedTouchAction>(
      routing_id, touch_action, unique_touch_event_id, ack_state));
}

void InputEventFilter::OnFilterAdded(IPC::Channel* channel) {
  io_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  sender_ = channel;
}

void InputEventFilter::OnFilterRemoved() {
  sender_ = nullptr;
}

void InputEventFilter::OnChannelClosing() {
  sender_ = nullptr;
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

  // If TimeTicks is not consistent across processes we cannot use the event's
  // platform timestamp in this process. Instead the time that the event is
  // received on the IO thread is used as the event's timestamp.
  base::TimeTicks received_time;
  if (!base::TimeTicks::IsConsistentAcrossProcesses())
    received_time = base::TimeTicks::Now();

  TRACE_EVENT0("input", "InputEventFilter::OnMessageReceived::InputMessage");

  int routing_id = message.routing_id();
  {
    base::AutoLock locked(routes_lock_);
    if (routes_.find(routing_id) == routes_.end()) {
      // |routes_| is based on the RenderView routing_id but the routing_id
      // may be from a RenderFrame. Messages from RenderFrames should be handled
      // synchronously with the associated RenderView as well.
      // Use the associated table to see if we have a mapping from
      // RenderFrame->RenderView if so use the queue for that routing id.
      // TODO(dtapuska): Input messages should NOT be sent to RenderFrames and
      // RenderViews; they should only goto one and this code will be
      // unnecessary as this would break for mojo which doesn't guarantee
      // ordering on different channels but Chrome IPC does.
      auto associated_routing_id = associated_routes_.find(routing_id);
      if (associated_routing_id == associated_routes_.end() ||
          routes_.find(associated_routing_id->second) == routes_.end()) {
        return false;
      }
      routing_id = associated_routing_id->second;
    }
  }

  bool postedTask = target_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputEventFilter::ForwardToHandler, this,
                                routing_id, message, received_time));
  LOG_IF(WARNING, !postedTask) << "PostTask failed";
  return true;
}

InputEventFilter::~InputEventFilter() {}

void InputEventFilter::ForwardToHandler(int associated_routing_id,
                                        const IPC::Message& message,
                                        base::TimeTicks received_time) {
  DCHECK(input_handler_manager_);
  DCHECK(target_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("input", "InputEventFilter::ForwardToHandler",
               "message_type", GetInputMessageTypeName(message));

  if (message.type() != InputMsg_HandleInputEvent::ID) {
    TRACE_EVENT_INSTANT0(
        "input",
        "InputEventFilter::ForwardToHandler::ForwardToMainListener",
        TRACE_EVENT_SCOPE_THREAD);
    input_handler_manager_->QueueClosureForMainThreadEventQueue(
        associated_routing_id, base::Bind(main_listener_, message));
    return;
  }

  InputMsg_HandleInputEvent::Param params;
  if (!InputMsg_HandleInputEvent::Read(&message, &params))
    return;
  ui::WebScopedInputEvent event =
      ui::WebInputEventTraits::Clone(*std::get<0>(params));
  ui::LatencyInfo latency_info = std::get<2>(params);
  InputEventDispatchType dispatch_type = std::get<3>(params);

  // HandleInputEvent is always sent to the RenderView routing ID
  // so it should be the same as the message routing ID.
  DCHECK(associated_routing_id == message.routing_id());
  DCHECK(event);
  DCHECK(dispatch_type == DISPATCH_TYPE_BLOCKING ||
         dispatch_type == DISPATCH_TYPE_NON_BLOCKING);

  if (!received_time.is_null())
    event->SetTimeStampSeconds(ui::EventTimeStampToSeconds(received_time));

  input_handler_manager_->HandleInputEvent(
      associated_routing_id, std::move(event), latency_info,
      base::Bind(&InputEventFilter::DidForwardToHandlerAndOverscroll, this,
                 associated_routing_id, dispatch_type));
};

void InputEventFilter::DidForwardToHandlerAndOverscroll(
    int routing_id,
    InputEventDispatchType dispatch_type,
    InputEventAckState ack_state,
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<DidOverscrollParams> overscroll_params) {
  bool send_ack = dispatch_type == DISPATCH_TYPE_BLOCKING;
  uint32_t unique_touch_event_id =
      ui::WebInputEventTraits::GetUniqueTouchEventId(*event);
  WebInputEvent::Type type = event->GetType();
  HandledEventCallback callback;
  if (send_ack) {
    callback = base::Bind(&InputEventFilter::SendInputEventAck, this,
                          routing_id, type, unique_touch_event_id);
  }

  if (ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
      ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
      ack_state == INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    DCHECK(!overscroll_params);
    RouteQueueMap::iterator iter = route_queues_.find(routing_id);
    if (iter != route_queues_.end()) {
      iter->second->HandleEvent(std::move(event), latency_info, dispatch_type,
                                ack_state, std::move(callback));
      return;
    }
  }
  if (callback) {
    std::move(callback).Run(ack_state, latency_info,
                            std::move(overscroll_params), base::nullopt);
  }
}

void InputEventFilter::SendInputEventAck(
    int routing_id,
    blink::WebInputEvent::Type event_type,
    int unique_touch_event_id,
    InputEventAckState ack_state,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params,
    base::Optional<cc::TouchAction> touch_action) {
  bool main_thread = main_task_runner_->BelongsToCurrentThread();

  InputEventAck ack(main_thread ? InputEventAckSource::MAIN_THREAD
                                : InputEventAckSource::COMPOSITOR_THREAD,
                    event_type, ack_state, latency_info,
                    std::move(overscroll_params), unique_touch_event_id,
                    touch_action);
  SendMessage(std::unique_ptr<IPC::Message>(
      new InputHostMsg_HandleInputEvent_ACK(routing_id, ack)));
}

void InputEventFilter::SendMessage(std::unique_ptr<IPC::Message> message) {
  CHECK(io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputEventFilter::SendMessageOnIOThread, this,
                                base::Passed(&message))))
      << "PostTask failed";
}

void InputEventFilter::SendMessageOnIOThread(
    std::unique_ptr<IPC::Message> message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!sender_)
    return;  // Filter was removed.

  bool success = sender_->Send(message.release());
  if (success)
    return;
  static size_t s_send_failure_count_ = 0;
  s_send_failure_count_++;

  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "input-event-filter-send-failure", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(crash_key,
                                 base::IntToString(s_send_failure_count_));
}

}  // namespace content
