// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_thread.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "content/renderer/gpu/input_event_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorInputHandler.h"

using WebKit::WebCompositorInputHandler;
using WebKit::WebInputEvent;

//------------------------------------------------------------------------------

class CompositorThread::InputHandlerWrapper
    : public WebKit::WebCompositorClient {
 public:
  InputHandlerWrapper(CompositorThread* compositor_thread,
                      int routing_id,
                      WebKit::WebCompositorInputHandler* input_handler)
      : compositor_thread_(compositor_thread),
        routing_id_(routing_id),
        input_handler_(input_handler) {
    input_handler_->setClient(this);
  }

  virtual ~InputHandlerWrapper() {
    input_handler_->setClient(NULL);
  }

  int routing_id() const { return routing_id_; }
  WebKit::WebCompositorInputHandler* input_handler() const {
    return input_handler_;
  }

  // WebCompositorClient methods:

  virtual void willShutdown() {
    compositor_thread_->RemoveInputHandler(routing_id_);
  }

  virtual void didHandleInputEvent() {
    compositor_thread_->filter_->DidHandleInputEvent();
  }

  virtual void didNotHandleInputEvent(bool send_to_widget) {
    compositor_thread_->filter_->DidNotHandleInputEvent(send_to_widget);
  }

 private:
  CompositorThread* compositor_thread_;
  int routing_id_;
  WebKit::WebCompositorInputHandler* input_handler_;

  DISALLOW_COPY_AND_ASSIGN(InputHandlerWrapper);
};

//------------------------------------------------------------------------------

CompositorThread::CompositorThread(IPC::Channel::Listener* main_listener)
    : thread_("Compositor") {
  filter_ =
      new InputEventFilter(main_listener,
                           thread_.message_loop()->message_loop_proxy(),
                           base::Bind(&CompositorThread::HandleInputEvent,
                                      base::Unretained(this)));
}

CompositorThread::~CompositorThread() {
}

IPC::ChannelProxy::MessageFilter* CompositorThread::GetMessageFilter() const {
  return filter_;
}

void CompositorThread::AddInputHandler(int routing_id, int input_handler_id) {
  if (thread_.message_loop() != MessageLoop::current()) {
    thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&CompositorThread::AddInputHandler, base::Unretained(this),
                   routing_id, input_handler_id));
    return;
  }

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

  TRACE_EVENT0("CompositorThread::AddInputHandler", "AddingRoute");
  filter_->AddRoute(routing_id);
  input_handlers_[routing_id] =
      make_linked_ptr(new InputHandlerWrapper(this, routing_id, input_handler));
}

void CompositorThread::RemoveInputHandler(int routing_id) {
  DCHECK(thread_.message_loop() == MessageLoop::current());

  TRACE_EVENT0("CompositorThread::RemoveInputHandler", "RemovingRoute");

  filter_->RemoveRoute(routing_id);
  input_handlers_.erase(routing_id);
}

void CompositorThread::HandleInputEvent(
    int routing_id,
    const WebInputEvent* input_event) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  InputHandlerMap::iterator it = input_handlers_.find(routing_id);
  if (it == input_handlers_.end()) {
    TRACE_EVENT0("CompositorThread::HandleInputEvent", "NoInputHandlerFound");
    // Oops, we no longer have an interested input handler..
    filter_->DidNotHandleInputEvent(true);
    return;
  }

  it->second->input_handler()->handleInputEvent(*input_event);
}
