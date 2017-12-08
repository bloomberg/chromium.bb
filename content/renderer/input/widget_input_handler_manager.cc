// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/widget_input_handler_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/input_messages.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input/widget_input_handler_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCoalescedInputEvent.h"
#include "third_party/WebKit/public/platform/WebKeyboardEvent.h"
#include "third_party/WebKit/public/platform/scheduler/renderer/renderer_scheduler.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "ui/events/base_event_utils.h"

namespace content {
namespace {
void CallCallback(mojom::WidgetInputHandler::DispatchEventCallback callback,
                  InputEventAckState ack_state,
                  const ui::LatencyInfo& latency_info,
                  std::unique_ptr<ui::DidOverscrollParams> overscroll_params,
                  base::Optional<cc::TouchAction> touch_action) {
  std::move(callback).Run(
      InputEventAckSource::MAIN_THREAD, latency_info, ack_state,
      overscroll_params
          ? base::Optional<ui::DidOverscrollParams>(*overscroll_params)
          : base::nullopt,
      touch_action);
}

}  // namespace

scoped_refptr<WidgetInputHandlerManager> WidgetInputHandlerManager::Create(
    base::WeakPtr<RenderWidget> render_widget,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    blink::scheduler::RendererScheduler* renderer_scheduler) {
  scoped_refptr<WidgetInputHandlerManager> manager =
      new WidgetInputHandlerManager(std::move(render_widget),
                                    std::move(compositor_task_runner),
                                    renderer_scheduler);
  manager->Init();
  return manager;
}

WidgetInputHandlerManager::WidgetInputHandlerManager(
    base::WeakPtr<RenderWidget> render_widget,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    blink::scheduler::RendererScheduler* renderer_scheduler)
    : render_widget_(render_widget),
      renderer_scheduler_(renderer_scheduler),
      input_event_queue_(render_widget->GetInputEventQueue()),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      compositor_task_runner_(compositor_task_runner) {
}

void WidgetInputHandlerManager::Init() {
  if (compositor_task_runner_) {
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WidgetInputHandlerManager::InitOnCompositorThread, this,
            render_widget_->compositor()->GetInputHandler(),
            render_widget_->compositor_deps()->IsScrollAnimatorEnabled()));
  }
}

WidgetInputHandlerManager::~WidgetInputHandlerManager() {}

void WidgetInputHandlerManager::AddAssociatedInterface(
    mojom::WidgetInputHandlerAssociatedRequest request,
    mojom::WidgetInputHandlerHostPtr host) {
  if (compositor_task_runner_) {
    associated_host_ =
        mojo::ThreadSafeInterfacePtr<mojom::WidgetInputHandlerHost>::Create(
            host.PassInterface(), compositor_task_runner_);
    // Mojo channel bound on compositor thread.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WidgetInputHandlerManager::BindAssociatedChannel, this,
                       std::move(request)));
  } else {
    associated_host_ =
        mojo::ThreadSafeInterfacePtr<mojom::WidgetInputHandlerHost>::Create(
            std::move(host));
    // Mojo channel bound on main thread.
    BindAssociatedChannel(std::move(request));
  }
}

void WidgetInputHandlerManager::AddInterface(
    mojom::WidgetInputHandlerRequest request,
    mojom::WidgetInputHandlerHostPtr host) {
  if (compositor_task_runner_) {
    host_ = mojo::ThreadSafeInterfacePtr<mojom::WidgetInputHandlerHost>::Create(
        host.PassInterface(), compositor_task_runner_);
    // Mojo channel bound on compositor thread.
    compositor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WidgetInputHandlerManager::BindChannel, this,
                                  std::move(request)));
  } else {
    host_ = mojo::ThreadSafeInterfacePtr<mojom::WidgetInputHandlerHost>::Create(
        std::move(host));
    // Mojo channel bound on main thread.
    BindChannel(std::move(request));
  }
}

void WidgetInputHandlerManager::WillShutdown() {
  input_handler_proxy_.reset();
}

void WidgetInputHandlerManager::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderWidget::TransferActiveWheelFlingAnimation,
                     render_widget_, params));
}

void WidgetInputHandlerManager::DispatchNonBlockingEventToMainThread(
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info) {
  DCHECK(input_event_queue_);
  input_event_queue_->HandleEvent(
      std::move(event), latency_info, DISPATCH_TYPE_NON_BLOCKING,
      INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING, HandledEventCallback());
}

std::unique_ptr<blink::WebGestureCurve>
WidgetInputHandlerManager::CreateFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  return blink::Platform::Current()->CreateFlingAnimationCurve(
      device_source, velocity, cumulative_scroll);
}

void WidgetInputHandlerManager::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity,
    const gfx::PointF& causal_event_viewport_point,
    const cc::OverscrollBehavior& overscroll_behavior) {
  mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  ui::DidOverscrollParams params;
  params.accumulated_overscroll = accumulated_overscroll;
  params.latest_overscroll_delta = latest_overscroll_delta;
  params.current_fling_velocity = current_fling_velocity;
  params.causal_event_viewport_point = causal_event_viewport_point;
  params.overscroll_behavior = overscroll_behavior;
  host->DidOverscroll(params);
}

void WidgetInputHandlerManager::DidStopFlinging() {
  mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  host->DidStopFlinging();
}

void WidgetInputHandlerManager::DidAnimateForInput() {
  renderer_scheduler_->DidAnimateForInputOnCompositorThread();
}

void WidgetInputHandlerManager::GenerateScrollBeginAndSendToMainThread(
    const blink::WebGestureEvent& update_event) {
  DCHECK_EQ(update_event.GetType(), blink::WebInputEvent::kGestureScrollUpdate);
  blink::WebGestureEvent scroll_begin(update_event);
  scroll_begin.SetType(blink::WebInputEvent::kGestureScrollBegin);
  scroll_begin.data.scroll_begin.inertial_phase =
      update_event.data.scroll_update.inertial_phase;
  scroll_begin.data.scroll_begin.delta_x_hint =
      update_event.data.scroll_update.delta_x;
  scroll_begin.data.scroll_begin.delta_y_hint =
      update_event.data.scroll_update.delta_y;
  scroll_begin.data.scroll_begin.delta_hint_units =
      update_event.data.scroll_update.delta_units;

  DispatchNonBlockingEventToMainThread(
      ui::WebInputEventTraits::Clone(scroll_begin), ui::LatencyInfo());
}

void WidgetInputHandlerManager::SetWhiteListedTouchAction(
    cc::TouchAction touch_action,
    uint32_t unique_touch_event_id,
    ui::InputHandlerProxy::EventDisposition event_disposition) {
  mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost();
  if (!host)
    return;
  InputEventAckState ack_state = InputEventDispositionToAck(event_disposition);
  host->SetWhiteListedTouchAction(touch_action, unique_touch_event_id,
                                  ack_state);
}

void WidgetInputHandlerManager::ProcessTouchAction(
    cc::TouchAction touch_action) {
  // Cancel the touch timeout on TouchActionNone since it is a good hint
  // that author doesn't want scrolling.
  if (touch_action == cc::TouchAction::kTouchActionNone) {
    if (mojom::WidgetInputHandlerHost* host = GetWidgetInputHandlerHost())
      host->CancelTouchTimeout();
  }
}

mojom::WidgetInputHandlerHost*
WidgetInputHandlerManager::GetWidgetInputHandlerHost() {
  if (associated_host_)
    return associated_host_.get()->get();
  if (host_)
    return host_.get()->get();
  return nullptr;
}

void WidgetInputHandlerManager::ObserveGestureEventOnMainThread(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  if (compositor_task_runner_) {
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WidgetInputHandlerManager::ObserveGestureEventOnCompositorThread,
            this, gesture_event, scroll_result));
  }
}

void WidgetInputHandlerManager::DispatchEvent(
    std::unique_ptr<content::InputEvent> event,
    mojom::WidgetInputHandler::DispatchEventCallback callback) {
  if (!event || !event->web_event) {
    // Call |callback| if it was available indicating this event wasn't
    // handled.
    if (callback) {
      std::move(callback).Run(
          InputEventAckSource::MAIN_THREAD, ui::LatencyInfo(),
          INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt, base::nullopt);
    }
    return;
  }

  // If TimeTicks is not consistent across processes we cannot use the event's
  // platform timestamp in this process. Instead use the time that the event is
  // received as the event's timestamp.
  if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
    event->web_event->SetTimeStampSeconds(
        ui::EventTimeStampToSeconds(base::TimeTicks::Now()));
  }

  if (compositor_task_runner_) {
    // If the input_handler_proxy has disappeared ensure we just ack event.
    if (!input_handler_proxy_) {
      if (callback) {
        std::move(callback).Run(
            InputEventAckSource::MAIN_THREAD, ui::LatencyInfo(),
            INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt, base::nullopt);
      }
      return;
    }
    CHECK(!main_thread_task_runner_->BelongsToCurrentThread());
    input_handler_proxy_->HandleInputEventWithLatencyInfo(
        std::move(event->web_event), event->latency_info,
        base::BindOnce(
            &WidgetInputHandlerManager::DidHandleInputEventAndOverscroll, this,
            std::move(callback)));
  } else {
    HandleInputEvent(std::move(event->web_event), event->latency_info,
                     std::move(callback));
  }
}

void WidgetInputHandlerManager::InitOnCompositorThread(
    const base::WeakPtr<cc::InputHandler>& input_handler,
    bool smooth_scroll_enabled) {
  input_handler_proxy_ = std::make_unique<ui::InputHandlerProxy>(
      input_handler.get(), this,
      base::FeatureList::IsEnabled(features::kTouchpadAndWheelScrollLatching));
  input_handler_proxy_->set_smooth_scroll_enabled(smooth_scroll_enabled);
}

void WidgetInputHandlerManager::BindAssociatedChannel(
    mojom::WidgetInputHandlerAssociatedRequest request) {
  if (!request.is_pending())
    return;
  // Don't pass the |input_event_queue_| on if we don't have a
  // |compositor_task_runner_| as events might get out of order.
  WidgetInputHandlerImpl* handler = new WidgetInputHandlerImpl(
      this, main_thread_task_runner_,
      compositor_task_runner_ ? input_event_queue_ : nullptr, render_widget_);
  handler->SetAssociatedBinding(std::move(request));
}

void WidgetInputHandlerManager::BindChannel(
    mojom::WidgetInputHandlerRequest request) {
  if (!request.is_pending())
    return;
  // Don't pass the |input_event_queue_| on if we don't have a
  // |compositor_task_runner_| as events might get out of order.
  WidgetInputHandlerImpl* handler = new WidgetInputHandlerImpl(
      this, main_thread_task_runner_,
      compositor_task_runner_ ? input_event_queue_ : nullptr, render_widget_);
  handler->SetBinding(std::move(request));
}

void WidgetInputHandlerManager::HandleInputEvent(
    const ui::WebScopedInputEvent& event,
    const ui::LatencyInfo& latency,
    mojom::WidgetInputHandler::DispatchEventCallback callback) {
  if (!render_widget_ || render_widget_->is_swapped_out() ||
      render_widget_->IsClosing()) {
    if (callback) {
      std::move(callback).Run(InputEventAckSource::MAIN_THREAD, latency,
                              INPUT_EVENT_ACK_STATE_NOT_CONSUMED, base::nullopt,
                              base::nullopt);
    }
    return;
  }
  auto send_callback = base::BindOnce(
      &WidgetInputHandlerManager::HandledInputEvent, this, std::move(callback));

  blink::WebCoalescedInputEvent coalesced_event(*event);
  render_widget_->HandleInputEvent(coalesced_event, latency,
                                   std::move(send_callback));
}

void WidgetInputHandlerManager::DidHandleInputEventAndOverscroll(
    mojom::WidgetInputHandler::DispatchEventCallback callback,
    ui::InputHandlerProxy::EventDisposition event_disposition,
    ui::WebScopedInputEvent input_event,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params) {
  InputEventAckState ack_state = InputEventDispositionToAck(event_disposition);
  switch (ack_state) {
    case INPUT_EVENT_ACK_STATE_CONSUMED:
      renderer_scheduler_->DidHandleInputEventOnCompositorThread(
          *input_event, blink::scheduler::RendererScheduler::InputEventState::
                            EVENT_CONSUMED_BY_COMPOSITOR);
      break;
    case INPUT_EVENT_ACK_STATE_NOT_CONSUMED:
    case INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING:
      renderer_scheduler_->DidHandleInputEventOnCompositorThread(
          *input_event, blink::scheduler::RendererScheduler::InputEventState::
                            EVENT_FORWARDED_TO_MAIN_THREAD);
      break;
    default:
      break;
  }
  if (ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING ||
      ack_state == INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING_DUE_TO_FLING ||
      ack_state == INPUT_EVENT_ACK_STATE_NOT_CONSUMED) {
    DCHECK(!overscroll_params);
    InputEventDispatchType dispatch_type = callback.is_null()
                                               ? DISPATCH_TYPE_NON_BLOCKING
                                               : DISPATCH_TYPE_BLOCKING;
    HandledEventCallback handled_event =
        base::BindOnce(&WidgetInputHandlerManager::HandledInputEvent, this,
                       std::move(callback));
    input_event_queue_->HandleEvent(std::move(input_event), latency_info,
                                    dispatch_type, ack_state,
                                    std::move(handled_event));
    return;
  }
  if (callback) {
    std::move(callback).Run(
        InputEventAckSource::COMPOSITOR_THREAD, latency_info, ack_state,
        overscroll_params
            ? base::Optional<ui::DidOverscrollParams>(*overscroll_params)
            : base::nullopt,
        base::nullopt);
  }
}

void WidgetInputHandlerManager::HandledInputEvent(
    mojom::WidgetInputHandler::DispatchEventCallback callback,
    InputEventAckState ack_state,
    const ui::LatencyInfo& latency_info,
    std::unique_ptr<ui::DidOverscrollParams> overscroll_params,
    base::Optional<cc::TouchAction> touch_action) {
  if (!callback)
    return;

  // This method is called from either the main thread or the compositor thread.
  bool is_compositor_thread = compositor_task_runner_ &&
                              compositor_task_runner_->BelongsToCurrentThread();

  // If there is a compositor task runner and the current thread isn't the
  // compositor thread proxy it over to the compositor thread.
  if (compositor_task_runner_ && !is_compositor_thread) {
    compositor_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(CallCallback, std::move(callback), ack_state,
                                  latency_info, std::move(overscroll_params),
                                  touch_action));
  } else {
    // Otherwise call the callback immediately.
    std::move(callback).Run(
        is_compositor_thread ? InputEventAckSource::COMPOSITOR_THREAD
                             : InputEventAckSource::MAIN_THREAD,
        latency_info, ack_state,
        overscroll_params
            ? base::Optional<ui::DidOverscrollParams>(*overscroll_params)
            : base::nullopt,
        touch_action);
  }
}

void WidgetInputHandlerManager::ObserveGestureEventOnCompositorThread(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  if (!input_handler_proxy_)
    return;
  DCHECK(input_handler_proxy_->scroll_elasticity_controller());
  input_handler_proxy_->scroll_elasticity_controller()
      ->ObserveGestureEventAndResult(gesture_event, scroll_result);
}

}  // namespace content
