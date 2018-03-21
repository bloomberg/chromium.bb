// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/input/input_handler.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "content/renderer/input/input_handler_wrapper.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebInputEvent;
using ui::InputHandlerProxy;
using blink::scheduler::RendererScheduler;

namespace content {

InputEventAckState InputEventDispositionToAck(
    InputHandlerProxy::EventDisposition disposition) {
  switch (disposition) {
    case InputHandlerProxy::DID_HANDLE:
      return INPUT_EVENT_ACK_STATE_CONSUMED;
    case InputHandlerProxy::DID_NOT_HANDLE:
      return INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
    case InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING:
      return INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING;
    case InputHandlerProxy::DROP_EVENT:
      return INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    case InputHandlerProxy::DID_HANDLE_NON_BLOCKING:
      return INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING;
    case InputHandlerProxy::DID_HANDLE_SHOULD_BUBBLE:
      return INPUT_EVENT_ACK_STATE_CONSUMED_SHOULD_BUBBLE;
  }
  NOTREACHED();
  return INPUT_EVENT_ACK_STATE_UNKNOWN;
}

InputHandlerManager::InputHandlerManager(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    InputHandlerManagerClient* client,
    SynchronousInputHandlerProxyClient* sync_handler_client,
    blink::scheduler::RendererScheduler* renderer_scheduler)
    : task_runner_(task_runner),
      client_(client),
      synchronous_handler_proxy_client_(sync_handler_client),
      renderer_scheduler_(renderer_scheduler),
      weak_ptr_factory_(this) {
  DCHECK(client_);
  client_->SetInputHandlerManager(this);
}

InputHandlerManager::~InputHandlerManager() {
  client_->SetInputHandlerManager(nullptr);
}

void InputHandlerManager::AddInputHandler(
    int routing_id,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const scoped_refptr<MainThreadEventQueue>& input_event_queue,
    const base::WeakPtr<RenderWidget>& render_widget,
    bool enable_smooth_scrolling) {
  if (task_runner_->BelongsToCurrentThread()) {
    AddInputHandlerOnCompositorThread(
        routing_id, base::ThreadTaskRunnerHandle::Get(), input_handler,
        input_event_queue, render_widget, enable_smooth_scrolling);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InputHandlerManager::AddInputHandlerOnCompositorThread,
                       base::Unretained(this), routing_id,
                       base::ThreadTaskRunnerHandle::Get(), input_handler,
                       input_event_queue, render_widget,
                       enable_smooth_scrolling));
  }
}

void InputHandlerManager::AddInputHandlerOnCompositorThread(
    int routing_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const scoped_refptr<MainThreadEventQueue>& input_event_queue,
    const base::WeakPtr<RenderWidget>& render_widget,
    bool enable_smooth_scrolling) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // The handler could be gone by this point if the compositor has shut down.
  if (!input_handler)
    return;

  // The same handler may be registered for a route multiple times.
  if (input_handlers_.count(routing_id) != 0)
    return;

  TRACE_EVENT1("input",
      "InputHandlerManager::AddInputHandlerOnCompositorThread",
      "result", "AddingRoute");
  std::unique_ptr<InputHandlerWrapper> wrapper(
      new InputHandlerWrapper(this, routing_id, main_task_runner, input_handler,
                              render_widget, enable_smooth_scrolling));
  client_->RegisterRoutingID(routing_id, input_event_queue);
  if (synchronous_handler_proxy_client_) {
    synchronous_handler_proxy_client_->DidAddSynchronousHandlerProxy(
        routing_id, wrapper->input_handler_proxy());
  }
  input_handlers_[routing_id] = std::move(wrapper);
}

void InputHandlerManager::RemoveInputHandler(int routing_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(input_handlers_.find(routing_id) != input_handlers_.end());

  TRACE_EVENT0("input", "InputHandlerManager::RemoveInputHandler");

  client_->UnregisterRoutingID(routing_id);
  if (synchronous_handler_proxy_client_) {
    synchronous_handler_proxy_client_->DidRemoveSynchronousHandlerProxy(
        routing_id);
  }
  input_handlers_.erase(routing_id);
}

void InputHandlerManager::RegisterAssociatedRenderFrameRoutingID(
    int render_frame_routing_id,
    int render_view_routing_id) {
  if (task_runner_->BelongsToCurrentThread()) {
    RegisterAssociatedRenderFrameRoutingIDOnCompositorThread(
        render_frame_routing_id, render_view_routing_id);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InputHandlerManager::
                RegisterAssociatedRenderFrameRoutingIDOnCompositorThread,
            base::Unretained(this), render_frame_routing_id,
            render_view_routing_id));
  }
}

void InputHandlerManager::
    RegisterAssociatedRenderFrameRoutingIDOnCompositorThread(
        int render_frame_routing_id,
        int render_view_routing_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->RegisterAssociatedRenderFrameRoutingID(render_frame_routing_id,
                                                  render_view_routing_id);
}

void InputHandlerManager::UnregisterRoutingID(int routing_id) {
  if (task_runner_->BelongsToCurrentThread()) {
    UnregisterRoutingIDOnCompositorThread(routing_id);
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &InputHandlerManager::UnregisterRoutingIDOnCompositorThread,
            base::Unretained(this), routing_id));
  }
}

void InputHandlerManager::UnregisterRoutingIDOnCompositorThread(
    int routing_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->UnregisterRoutingID(routing_id);
}

void InputHandlerManager::ObserveGestureEventAndResultOnMainThread(
    int routing_id,
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputHandlerManager::ObserveGestureEventAndResultOnCompositorThread,
          base::Unretained(this), routing_id, gesture_event, scroll_result));
}

void InputHandlerManager::ObserveGestureEventAndResultOnCompositorThread(
    int routing_id,
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  auto it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end())
    return;

  InputHandlerProxy* proxy = it->second->input_handler_proxy();
  DCHECK(proxy->scroll_elasticity_controller());
  proxy->scroll_elasticity_controller()->ObserveGestureEventAndResult(
      gesture_event, scroll_result);
}

void InputHandlerManager::HandleInputEvent(
    int routing_id,
    ui::WebScopedInputEvent input_event,
    const ui::LatencyInfo& latency_info,
    const InputEventAckStateCallback& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("input,benchmark,rail", "InputHandlerManager::HandleInputEvent",
               "type", WebInputEvent::GetName(input_event->GetType()));

  auto it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end()) {
    TRACE_EVENT1("input,rail", "InputHandlerManager::HandleInputEvent",
                 "result", "NoInputHandlerFound");
    // Oops, we no longer have an interested input handler..
    callback.Run(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, std::move(input_event),
                 latency_info, nullptr);
    return;
  }

  TRACE_EVENT1("input,rail", "InputHandlerManager::HandleInputEvent",
               "result", "EventSentToInputHandlerProxy");
  InputHandlerProxy* proxy = it->second->input_handler_proxy();
  proxy->HandleInputEventWithLatencyInfo(
      std::move(input_event), latency_info,
      base::BindOnce(&InputHandlerManager::DidHandleInputEventAndOverscroll,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void InputHandlerManager::QueueClosureForMainThreadEventQueue(
    int routing_id,
    const base::Closure& closure) {
  client_->QueueClosureForMainThreadEventQueue(routing_id, closure);
}

void InputHandlerManager::DidHandleInputEventAndOverscroll(
    const InputEventAckStateCallback& callback,
    InputHandlerProxy::EventDisposition event_disposition,
    ui::WebScopedInputEvent input_event,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params) {
  InputEventAckState input_event_ack_state =
      InputEventDispositionToAck(event_disposition);
  switch (input_event_ack_state) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
      renderer_scheduler_->DidHandleInputEventOnCompositorThread(
          *input_event,
          RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING:
      renderer_scheduler_->DidHandleInputEventOnCompositorThread(
          *input_event,
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      break;
    default:
      break;
  }
  callback.Run(input_event_ack_state, std::move(input_event), latency_info,
               std::move(overscroll_params));
}

void InputHandlerManager::DidOverscroll(int routing_id,
                                        const ui::DidOverscrollParams& params) {
  client_->DidOverscroll(routing_id, params);
}

void InputHandlerManager::DidStopFlinging(int routing_id) {
  client_->DidStopFlinging(routing_id);
}

void InputHandlerManager::DidAnimateForInput() {
  renderer_scheduler_->DidAnimateForInputOnCompositorThread();
}

void InputHandlerManager::DidStartScrollingViewport(int routing_id) {
  client_->DidStartScrollingViewport(routing_id);
}

void InputHandlerManager::DispatchNonBlockingEventToMainThread(
    int routing_id,
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->DispatchNonBlockingEventToMainThread(routing_id, std::move(event),
                                                latency_info);
}

void InputHandlerManager::SetWhiteListedTouchAction(
    int routing_id,
    cc::TouchAction touch_action,
    uint32_t unique_touch_event_id,
    InputHandlerProxy::EventDisposition event_disposition) {
  InputEventAckState input_event_ack_state =
      InputEventDispositionToAck(event_disposition);
  client_->SetWhiteListedTouchAction(
      routing_id, touch_action, unique_touch_event_id, input_event_ack_state);
}

}  // namespace content
