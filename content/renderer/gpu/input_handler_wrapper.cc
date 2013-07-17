// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/input_handler_wrapper.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/gpu/input_event_filter.h"
#include "content/renderer/gpu/input_handler_manager.h"
#include "third_party/WebKit/public/platform/Platform.h"

namespace content {

InputHandlerWrapper::InputHandlerWrapper(
    InputHandlerManager* input_handler_manager,
    int routing_id,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl)
    : input_handler_manager_(input_handler_manager),
      routing_id_(routing_id),
      input_handler_proxy_(input_handler.get()),
      main_loop_(main_loop),
      render_view_impl_(render_view_impl) {
  input_handler_proxy_.SetClient(this);
}

InputHandlerWrapper::~InputHandlerWrapper() {
  input_handler_proxy_.SetClient(NULL);
}

void InputHandlerWrapper::TransferActiveWheelFlingAnimation(
    const WebKit::WebActiveWheelFlingParameters& params) {
  main_loop_->PostTask(
      FROM_HERE,
      base::Bind(&RenderViewImpl::TransferActiveWheelFlingAnimation,
                 render_view_impl_,
                 params));
}

void InputHandlerWrapper::WillShutdown() {
  input_handler_manager_->RemoveInputHandler(routing_id_);
}

WebKit::WebGestureCurve* InputHandlerWrapper::CreateFlingAnimationCurve(
    int deviceSource,
    const WebKit::WebFloatPoint& velocity,
    const WebKit::WebSize& cumulative_scroll) {
  return WebKit::Platform::current()->createFlingAnimationCurve(
      deviceSource, velocity, cumulative_scroll);
}

void InputHandlerWrapper::DidOverscroll(const cc::DidOverscrollParams& params) {
  input_handler_manager_->DidOverscroll(routing_id_, params);
}

}  // namespace content
