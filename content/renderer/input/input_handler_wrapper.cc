// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_handler_wrapper.h"

#include "base/command_line.h"
#include "base/location.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "ui/events/blink/did_overscroll_params.h"

namespace content {

InputHandlerWrapper::InputHandlerWrapper(
    InputHandlerManager* input_handler_manager,
    int routing_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderWidget>& render_widget,
    bool enable_smooth_scrolling)
    : input_handler_manager_(input_handler_manager),
      routing_id_(routing_id),
      input_handler_proxy_(input_handler.get(),
                           this,
                           base::FeatureList::IsEnabled(
                               features::kTouchpadAndWheelScrollLatching)),
      main_task_runner_(main_task_runner),
      render_widget_(render_widget) {
  DCHECK(input_handler);
  input_handler_proxy_.set_smooth_scroll_enabled(enable_smooth_scrolling);
}

InputHandlerWrapper::~InputHandlerWrapper() {
}

void InputHandlerWrapper::NeedsMainFrame() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderWidget::SetNeedsMainFrame, render_widget_));
}

void InputHandlerWrapper::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderWidget::TransferActiveWheelFlingAnimation,
                     render_widget_, params));
}

void InputHandlerWrapper::DispatchNonBlockingEventToMainThread(
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency_info) {
  input_handler_manager_->DispatchNonBlockingEventToMainThread(
      routing_id_, std::move(event), latency_info);
}

void InputHandlerWrapper::WillShutdown() {
  input_handler_manager_->RemoveInputHandler(routing_id_);
}

std::unique_ptr<blink::WebGestureCurve>
InputHandlerWrapper::CreateFlingAnimationCurve(
    blink::WebGestureDevice deviceSource,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  return blink::Platform::Current()->CreateFlingAnimationCurve(
      deviceSource, velocity, cumulative_scroll);
}

void InputHandlerWrapper::DidOverscroll(
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::Vector2dF& latest_overscroll_delta,
    const gfx::Vector2dF& current_fling_velocity,
    const gfx::PointF& causal_event_viewport_point,
    const cc::OverscrollBehavior& overscroll_behavior) {
  ui::DidOverscrollParams params;
  params.accumulated_overscroll = accumulated_overscroll;
  params.latest_overscroll_delta = latest_overscroll_delta;
  params.current_fling_velocity = current_fling_velocity;
  params.causal_event_viewport_point = causal_event_viewport_point;
  params.overscroll_behavior = overscroll_behavior;
  input_handler_manager_->DidOverscroll(routing_id_, params);
}

void InputHandlerWrapper::DidStopFlinging() {
  input_handler_manager_->DidStopFlinging(routing_id_);
}

void InputHandlerWrapper::DidAnimateForInput() {
  input_handler_manager_->DidAnimateForInput();
}

void InputHandlerWrapper::GenerateScrollBeginAndSendToMainThread(
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

void InputHandlerWrapper::SetWhiteListedTouchAction(
    cc::TouchAction touch_action,
    uint32_t unique_touch_event_id,
    ui::InputHandlerProxy::EventDisposition event_disposition) {
  input_handler_manager_->SetWhiteListedTouchAction(
      routing_id_, touch_action, unique_touch_event_id, event_disposition);
}

}  // namespace content
