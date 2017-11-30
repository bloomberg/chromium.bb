// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/input/input_handler_manager.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"

namespace ui {
class InputHandlerProxy;
class InputHandlerProxyClient;
}

namespace content {

// This class lives on the compositor thread.
class InputHandlerWrapper : public ui::InputHandlerProxyClient {
 public:
  InputHandlerWrapper(
      InputHandlerManager* input_handler_manager,
      int routing_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderWidget>& render_widget,
      bool enable_smooth_scrolling);
  ~InputHandlerWrapper() override;

  int routing_id() const { return routing_id_; }
  ui::InputHandlerProxy* input_handler_proxy() {
    return &input_handler_proxy_;
  }

  void NeedsMainFrame();

  // InputHandlerProxyClient implementation.
  void WillShutdown() override;
  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) override;
  void DispatchNonBlockingEventToMainThread(
      ui::WebScopedInputEvent event,
      const ui::LatencyInfo& latency_info) override;
  std::unique_ptr<blink::WebGestureCurve> CreateFlingAnimationCurve(
      blink::WebGestureDevice deviceSource,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulativeScroll) override;
  void DidOverscroll(
      const gfx::Vector2dF& accumulated_overscroll,
      const gfx::Vector2dF& latest_overscroll_delta,
      const gfx::Vector2dF& current_fling_velocity,
      const gfx::PointF& causal_event_viewport_point,
      const cc::OverscrollBehavior& overscroll_behavior) override;
  void DidStopFlinging() override;
  void DidAnimateForInput() override;
  void GenerateScrollBeginAndSendToMainThread(
      const blink::WebGestureEvent& update_event) override;
  void SetWhiteListedTouchAction(
      cc::TouchAction touch_action,
      uint32_t unique_touch_event_id,
      ui::InputHandlerProxy::EventDisposition event_disposition) override;

 private:
  InputHandlerManager* input_handler_manager_;
  int routing_id_;
  ui::InputHandlerProxy input_handler_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Can only be accessed on the main thread.
  base::WeakPtr<RenderWidget> render_widget_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_
