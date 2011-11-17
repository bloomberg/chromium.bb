// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_INPUT_EVENT_FILTER_H_
#define CONTENT_RENDERER_GPU_INPUT_EVENT_FILTER_H_

#include <queue>
#include <set>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

// This class can be used to intercept ViewMsg_HandleInputEvent messages
// and have them be delivered to a target thread.  Input events are filtered
// based on routing_id (see AddRoute and RemoveRoute).
//
// The user of this class provides an instance of InputEventFilter::Handler,
// which will be passed WebInputEvents on the target thread.
//
class CONTENT_EXPORT InputEventFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  typedef base::Callback<void(int /*routing_id*/,
                              const WebKit::WebInputEvent*)> Handler;

  // The |handler| is invoked on the thread associated with |target_loop| to
  // handle input events matching the filtered routes.  In response, the
  // handler should call either DidHandleInputEvent or DidNotHandleInputEvent.
  // These may be called asynchronously to the handler invocation, but they
  // must be called on the target thread.
  //
  // If DidNotHandleInputEvent is called with send_to_widget set to true, then
  // the original ViewMsg_HandleInputEvent message will be delivered to
  // |main_listener| on the main thread.  (The "main thread" in this context is
  // the thread where the InputEventFilter was constructed.)  If send_to_widget
  // is true, then a ViewHostMsg_HandleInputEvent_ACK will not be generated,
  // leaving that responsibility up to the eventual handler on the main thread.
  //
  InputEventFilter(IPC::Channel::Listener* main_listener,
                   base::MessageLoopProxy* target_loop,
                   const Handler& handler);

  // Define the message routes to be filtered.
  void AddRoute(int routing_id);
  void RemoveRoute(int routing_id);

  // Called on the target thread by the Handler.
  void DidHandleInputEvent();
  void DidNotHandleInputEvent(bool send_to_widget);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Expects a ViewMsg_HandleInputEvent message.
  static const WebKit::WebInputEvent* CrackMessage(const IPC::Message& message);

 private:
  friend class IPC::ChannelProxy::MessageFilter;
  virtual ~InputEventFilter();

  void ForwardToMainListener(const IPC::Message& message);
  void ForwardToHandler(const IPC::Message& message);
  void SendACK(const IPC::Message& message, bool processed);
  void SendACKOnIOThread(int routing_id, WebKit::WebInputEvent::Type event_type,
                         bool processed);

  scoped_refptr<base::MessageLoopProxy> main_loop_;
  IPC::Channel::Listener* main_listener_;

  // The sender_ only gets invoked on the thread corresponding to io_loop_.
  scoped_refptr<base::MessageLoopProxy> io_loop_;
  IPC::Message::Sender* sender_;

  // The handler_ only gets Run on the thread corresponding to target_loop_.
  scoped_refptr<base::MessageLoopProxy> target_loop_;
  Handler handler_;
  std::queue<IPC::Message> messages_;

  // Protects access to routes_.
  base::Lock routes_lock_;

  // Indicates the routing_ids for which input events should be filtered.
  std::set<int> routes_;
};

#endif  // CONTENT_RENDERER_GPU_INPUT_EVENT_FILTER_H_
