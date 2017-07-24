// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_
#define CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_

#include "content/common/input/input_handler.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"

namespace blink {
namespace scheduler {
class RendererScheduler;
};  // namespace scheduler
};  // namespace blink

namespace content {
class MainThreadEventQueue;

// This class maintains the compositor InputHandlerProxy and is
// responsible for passing input events on the compositor and main threads.
// The lifecycle of this object matches that of the RenderWidget.
class WidgetInputHandlerManager
    : public base::RefCountedThreadSafe<WidgetInputHandlerManager>,
      public ui::InputHandlerProxyClient {
 public:
  WidgetInputHandlerManager(
      base::WeakPtr<RenderWidget> render_widget,
      IPC::Sender* legacy_host_channel,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      blink::scheduler::RendererScheduler* renderer_scheduler);

  void AddAssociatedInterface(
      mojom::WidgetInputHandlerAssociatedRequest interface_request);

  void AddInterface(mojom::WidgetInputHandlerRequest interface_request);

  // InputHandlerProxyClient overrides.
  void WillShutdown() override;
  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) override;
  void DispatchNonBlockingEventToMainThread(
      ui::WebScopedInputEvent event,
      const ui::LatencyInfo& latency_info) override;
  std::unique_ptr<blink::WebGestureCurve> CreateFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll) override;

  void DidOverscroll(const gfx::Vector2dF& accumulated_overscroll,
                     const gfx::Vector2dF& latest_overscroll_delta,
                     const gfx::Vector2dF& current_fling_velocity,
                     const gfx::PointF& causal_event_viewport_point) override;
  void DidStopFlinging() override;
  void DidAnimateForInput() override;
  void GenerateScrollBeginAndSendToMainThread(
      const blink::WebGestureEvent& update_event) override;
  void SetWhiteListedTouchAction(cc::TouchAction touch_action) override;

  void ObserveGestureEventOnMainThread(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void DispatchEvent(std::unique_ptr<content::InputEvent> event,
                     mojom::WidgetInputHandler::DispatchEventCallback callback);

 protected:
  friend class base::RefCountedThreadSafe<WidgetInputHandlerManager>;
  ~WidgetInputHandlerManager() override;

 private:
  void InitOnCompositorThread(
      const base::WeakPtr<cc::InputHandler>& input_handler,
      bool smooth_scroll_enabled);
  void BindAssociatedChannel(
      mojom::WidgetInputHandlerAssociatedRequest request);
  void BindChannel(mojom::WidgetInputHandlerRequest request);
  void HandleInputEvent(
      const ui::WebScopedInputEvent& event,
      const ui::LatencyInfo& latency,
      mojom::WidgetInputHandler::DispatchEventCallback callback);
  void DidHandleInputEventAndOverscroll(
      mojom::WidgetInputHandler::DispatchEventCallback callback,
      ui::InputHandlerProxy::EventDisposition event_disposition,
      ui::WebScopedInputEvent input_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params);
  void HandledInputEvent(
      mojom::WidgetInputHandler::DispatchEventCallback callback,
      InputEventAckState ack_state,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params);
  void ObserveGestureEventOnCompositorThread(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  base::WeakPtr<RenderWidget> render_widget_;
  blink::scheduler::RendererScheduler* renderer_scheduler_;
  scoped_refptr<MainThreadEventQueue> input_event_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  std::unique_ptr<ui::InputHandlerProxy> input_handler_proxy_;
  IPC::Sender* legacy_host_message_sender_;
  int legacy_host_message_routing_id_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WidgetInputHandlerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_WIDGET_INPUT_HANDLER_MANAGER_H_
