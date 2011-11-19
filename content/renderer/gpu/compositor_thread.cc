// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_thread.h"

#include "base/bind.h"
#include "content/renderer/gpu/input_event_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositorClient.h"

using WebKit::WebCompositor;
using WebKit::WebInputEvent;

//------------------------------------------------------------------------------

class CompositorThread::CompositorWrapper : public WebKit::WebCompositorClient {
 public:
  CompositorWrapper(CompositorThread* compositor_thread,
                    int routing_id,
                    WebKit::WebCompositor* compositor)
      : compositor_thread_(compositor_thread),
        routing_id_(routing_id),
        compositor_(compositor) {
    compositor_->setClient(this);
  }

  virtual ~CompositorWrapper() {
    compositor_->setClient(NULL);
  }

  int routing_id() const { return routing_id_; }
  WebKit::WebCompositor* compositor() const { return compositor_; }

  // WebCompositorClient methods:

  virtual void willShutdown() {
    compositor_thread_->RemoveCompositor(routing_id_);
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
  WebKit::WebCompositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(CompositorWrapper);
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

void CompositorThread::AddCompositor(int routing_id, int compositor_id) {
  if (thread_.message_loop() != MessageLoop::current()) {
    thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&CompositorThread::AddCompositor, base::Unretained(this),
                   routing_id, compositor_id));
    return;
  }

  WebCompositor* compositor = WebCompositor::fromIdentifier(compositor_id);
  if (!compositor)
    return;

  if (compositors_.count(routing_id) != 0) {
    // It's valid to call AddCompositor() for the same routing id with the same
    // compositor many times, but it's not valid to change the compositor for
    // a route.
    DCHECK_EQ(compositors_[routing_id]->compositor(), compositor);
    return;
  }

  filter_->AddRoute(routing_id);
  compositors_[routing_id] =
      make_linked_ptr(new CompositorWrapper(this, routing_id, compositor));
}

void CompositorThread::RemoveCompositor(int routing_id) {
  DCHECK(thread_.message_loop() == MessageLoop::current());

  filter_->RemoveRoute(routing_id);
  compositors_.erase(routing_id);
}

void CompositorThread::HandleInputEvent(
    int routing_id,
    const WebInputEvent* input_event) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  CompositorMap::iterator it = compositors_.find(routing_id);
  if (it == compositors_.end()) {
    // Oops, we no longer have an interested compositor.
    filter_->DidNotHandleInputEvent(true);
    return;
  }

  it->second->compositor()->handleInputEvent(*input_event);
}
