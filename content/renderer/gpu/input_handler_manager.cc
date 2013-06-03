// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/input_handler_manager.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "cc/input/input_handler.h"
#include "content/renderer/gpu/input_event_filter.h"
#include "content/renderer/gpu/input_handler_wrapper.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebActiveWheelFlingParameters.h"

using WebKit::WebInputEvent;

namespace content {

InputHandlerManager::InputHandlerManager(
    IPC::Listener* main_listener,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy)
    : message_loop_proxy_(message_loop_proxy) {
  filter_ =
      new InputEventFilter(main_listener,
                           message_loop_proxy,
                           base::Bind(&InputHandlerManager::HandleInputEvent,
                                      base::Unretained(this)));
}

InputHandlerManager::~InputHandlerManager() {
}

IPC::ChannelProxy::MessageFilter*
InputHandlerManager::GetMessageFilter() const {
  return filter_.get();
}

void InputHandlerManager::AddInputHandler(
    int routing_id,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());

  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&InputHandlerManager::AddInputHandlerOnCompositorThread,
                 base::Unretained(this),
                 routing_id,
                 base::MessageLoopProxy::current(),
                 input_handler,
                 render_view_impl));
}

void InputHandlerManager::AddInputHandlerOnCompositorThread(
    int routing_id,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    const base::WeakPtr<cc::InputHandler>& input_handler,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  // The handler could be gone by this point if the compositor has shut down.
  if (!input_handler.get())
    return;

  // The same handler may be registered for a route multiple times.
  if (input_handlers_.count(routing_id) != 0)
    return;

  TRACE_EVENT0("InputHandlerManager::AddInputHandler", "AddingRoute");
  filter_->AddRoute(routing_id);
  input_handlers_[routing_id] =
      make_scoped_refptr(new InputHandlerWrapper(this,
          routing_id, main_loop, input_handler, render_view_impl));
}

void InputHandlerManager::RemoveInputHandler(int routing_id) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  TRACE_EVENT0("InputHandlerManager::RemoveInputHandler", "RemovingRoute");

  filter_->RemoveRoute(routing_id);
  input_handlers_.erase(routing_id);
}

void InputHandlerManager::HandleInputEvent(
    int routing_id,
    const WebInputEvent* input_event) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  InputHandlerMap::iterator it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end()) {
    TRACE_EVENT0("InputHandlerManager::HandleInputEvent",
                 "NoInputHandlerFound");
    // Oops, we no longer have an interested input handler..
    filter_->DidNotHandleInputEvent(true);
    return;
  }

  it->second->input_handler_proxy()->HandleInputEvent(*input_event);
}

}  // namespace content
