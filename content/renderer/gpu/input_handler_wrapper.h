// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_INPUT_HANDLER_WRAPPER_H_
#define CONTENT_RENDERER_GPU_INPUT_HANDLER_WRAPPER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/gpu/input_handler_manager.h"
#include "content/renderer/gpu/input_handler_proxy.h"
#include "content/renderer/gpu/input_handler_proxy_client.h"

namespace content {

// This class lives on the compositor thread.
class InputHandlerWrapper
    : public InputHandlerProxyClient,
      public base::RefCountedThreadSafe<InputHandlerWrapper> {
 public:
  InputHandlerWrapper(InputHandlerManager* input_handler_manager,
                      int routing_id,
                      const scoped_refptr<base::MessageLoopProxy>& main_loop,
                      const base::WeakPtr<cc::InputHandler>& input_handler,
                      const base::WeakPtr<RenderViewImpl>& render_view_impl);

  int routing_id() const { return routing_id_; }
  InputHandlerProxy* input_handler_proxy() { return &input_handler_proxy_; }

  // InputHandlerProxyClient implementation.
  virtual void WillShutdown() OVERRIDE;
  virtual void TransferActiveWheelFlingAnimation(
      const WebKit::WebActiveWheelFlingParameters& params) OVERRIDE;
  virtual WebKit::WebGestureCurve* CreateFlingAnimationCurve(
      int deviceSource,
      const WebKit::WebFloatPoint& velocity,
      const WebKit::WebSize& cumulativeScroll) OVERRIDE;
  virtual void DidOverscroll(const cc::DidOverscrollParams& params) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<InputHandlerWrapper>;

  virtual ~InputHandlerWrapper();

  InputHandlerManager* input_handler_manager_;
  int routing_id_;
  InputHandlerProxy input_handler_proxy_;
  scoped_refptr<base::MessageLoopProxy> main_loop_;

  // Can only be accessed on the main thread.
  base::WeakPtr<RenderViewImpl> render_view_impl_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_INPUT_HANDLER_WRAPPER_H_
