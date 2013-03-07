// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/input_handler_manager.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/renderer/gpu/input_event_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebActiveWheelFlingParameters.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorInputHandler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorInputHandlerClient.h"

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
#include <sys/resource.h>
#endif

using WebKit::WebCompositorInputHandler;
using WebKit::WebInputEvent;

namespace content {

//------------------------------------------------------------------------------

class InputHandlerManager::InputHandlerWrapper
    : public WebKit::WebCompositorInputHandlerClient,
      public base::RefCountedThreadSafe<InputHandlerWrapper> {
 public:
  InputHandlerWrapper(InputHandlerManager* input_handler_manager,
                      int routing_id,
                      WebKit::WebCompositorInputHandler* input_handler,
                      const scoped_refptr<base::MessageLoopProxy>& main_loop,
                      const base::WeakPtr<RenderViewImpl>& render_view_impl)
      : input_handler_manager_(input_handler_manager),
        routing_id_(routing_id),
        input_handler_(input_handler),
        main_loop_(main_loop),
        render_view_impl_(render_view_impl) {
    input_handler_->setClient(this);
  }

  virtual void transferActiveWheelFlingAnimation(
      const WebKit::WebActiveWheelFlingParameters& params) {
    main_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RenderViewImpl::TransferActiveWheelFlingAnimation,
                   render_view_impl_, params));
  }

  int routing_id() const { return routing_id_; }
  WebKit::WebCompositorInputHandler* input_handler() const {
    return input_handler_;
  }

  // WebCompositorInputHandlerClient methods:

  virtual void willShutdown() {
    input_handler_manager_->RemoveInputHandler(routing_id_);
  }

  virtual void didHandleInputEvent() {
    input_handler_manager_->filter_->DidHandleInputEvent();
  }

  virtual void didNotHandleInputEvent(bool send_to_widget) {
    input_handler_manager_->filter_->DidNotHandleInputEvent(send_to_widget);
  }

 private:
  friend class base::RefCountedThreadSafe<InputHandlerWrapper>;

  virtual ~InputHandlerWrapper() {
    input_handler_->setClient(NULL);
  }

  InputHandlerManager* input_handler_manager_;
  int routing_id_;
  WebKit::WebCompositorInputHandler* input_handler_;
  scoped_refptr<base::MessageLoopProxy> main_loop_;

  // Can only be accessed on the main thread.
  base::WeakPtr<RenderViewImpl> render_view_impl_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

//------------------------------------------------------------------------------

#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
namespace {
void SetHighThreadPriority() {
  int nice_value = -6; // High priority.
  setpriority(PRIO_PROCESS, base::PlatformThread::CurrentId(), nice_value);
}
}
#endif

InputHandlerManager::InputHandlerManager(
    IPC::Listener* main_listener,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy)
    : message_loop_proxy_(message_loop_proxy) {
  filter_ =
      new InputEventFilter(main_listener,
                           message_loop_proxy,
                           base::Bind(&InputHandlerManager::HandleInputEvent,
                                      base::Unretained(this)));
#if defined(OS_ANDROID)
// TODO(epenner): Move thread priorities to base. (crbug.com/170549)
  message_loop_proxy->PostTask(FROM_HERE, base::Bind(&SetHighThreadPriority));
#endif
}

InputHandlerManager::~InputHandlerManager() {
}

IPC::ChannelProxy::MessageFilter*
InputHandlerManager::GetMessageFilter() const {
  return filter_;
}

void InputHandlerManager::AddInputHandler(
    int routing_id, int input_handler_id,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  DCHECK(!message_loop_proxy_->BelongsToCurrentThread());

  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&InputHandlerManager::AddInputHandlerOnCompositorThread,
                 base::Unretained(this),
                 routing_id,
                 input_handler_id,
                 base::MessageLoopProxy::current(),
                 render_view_impl));
}

void InputHandlerManager::AddInputHandlerOnCompositorThread(
    int routing_id, int input_handler_id,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    const base::WeakPtr<RenderViewImpl>& render_view_impl) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  WebCompositorInputHandler* input_handler =
      WebCompositorInputHandler::fromIdentifier(input_handler_id);
  if (!input_handler)
    return;

  if (input_handlers_.count(routing_id) != 0) {
    // It's valid to call AddInputHandler() for the same routing id with the
    // same input_handler many times, but it's not valid to change the
    // input_handler for a route.
    DCHECK_EQ(input_handlers_[routing_id]->input_handler(), input_handler);
    return;
  }

  TRACE_EVENT0("InputHandlerManager::AddInputHandler", "AddingRoute");
  filter_->AddRoute(routing_id);
  input_handlers_[routing_id] =
      make_scoped_refptr(new InputHandlerWrapper(this,
          routing_id, input_handler, main_loop, render_view_impl));
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

  it->second->input_handler()->handleInputEvent(*input_event);
}

}  // namespace content
