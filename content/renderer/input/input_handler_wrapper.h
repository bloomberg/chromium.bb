// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_
#define CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_

#include "base/memory/weak_ptr.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input/input_handler_proxy.h"
#include "content/renderer/input/input_handler_proxy_client.h"

namespace content {

// This class lives on the compositor thread.
class InputHandlerWrapper : public InputHandlerProxyClient {
 public:
  InputHandlerWrapper(InputHandlerManager* input_handler_manager,
                      int routing_id,
                      const scoped_refptr<base::MessageLoopProxy>& main_loop,
                      const base::WeakPtr<cc::InputHandler>& input_handler,
                      const base::WeakPtr<RenderViewImpl>& render_view_impl);
  ~InputHandlerWrapper() override;

  int routing_id() const { return routing_id_; }
  InputHandlerProxy* input_handler_proxy() { return &input_handler_proxy_; }

  // InputHandlerProxyClient implementation.
  void WillShutdown() override;
  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params) override;
  blink::WebGestureCurve* CreateFlingAnimationCurve(
      blink::WebGestureDevice deviceSource,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulativeScroll) override;
  void DidOverscroll(const DidOverscrollParams& params) override;
  void DidStopFlinging() override;
  void DidReceiveInputEvent(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInput() override;

 private:
  InputHandlerManager* input_handler_manager_;
  int routing_id_;
  InputHandlerProxy input_handler_proxy_;
  scoped_refptr<base::MessageLoopProxy> main_loop_;

  // Can only be accessed on the main thread.
  base::WeakPtr<RenderViewImpl> render_view_impl_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_HANDLER_WRAPPER_H_
