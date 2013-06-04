// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_INPUT_EVENT_FILTER_H_
#define CONTENT_RENDERER_GPU_INPUT_EVENT_FILTER_H_

#include <queue>
#include <set>

#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/port/common/input_event_ack_state.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

// This class can be used to intercept InputMsg_HandleInputEvent messages
// and have them be delivered to a target thread.  Input events are filtered
// based on routing_id (see AddRoute and RemoveRoute).
//
// The user of this class provides an instance of InputEventFilter::Handler,
// which will be passed WebInputEvents on the target thread.
//

namespace content {

class CONTENT_EXPORT InputEventFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  typedef base::Callback<InputEventAckState(
      int /*routing_id*/,
      const WebKit::WebInputEvent*)> Handler;

  // The |handler| is invoked on the thread associated with |target_loop| to
  // handle input events matching the filtered routes.
  //
  // If INPUT_EVENT_ACK_STATE_NOT_CONSUMED is returned by the handler,
  // the original InputMsg_HandleInputEvent message will be delivered to
  // |main_listener| on the main thread.  (The "main thread" in this context is
  // the thread where the InputEventFilter was constructed.)  The responsibility
  // is left to the eventual handler to deliver the corresponding
  // InputHostMsg_HandleInputEvent_ACK.
  //
  InputEventFilter(IPC::Listener* main_listener,
                   const scoped_refptr<base::MessageLoopProxy>& target_loop,
                   const Handler& handler);

  // Define the message routes to be filtered.
  void AddRoute(int routing_id);
  void RemoveRoute(int routing_id);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Expects a InputMsg_HandleInputEvent message.
  static const WebKit::WebInputEvent* CrackMessage(const IPC::Message& message);

 private:
  friend class IPC::ChannelProxy::MessageFilter;
  virtual ~InputEventFilter();

  void ForwardToMainListener(const IPC::Message& message);
  void ForwardToHandler(const IPC::Message& message);
  void SendACK(const IPC::Message& message, InputEventAckState ack_result);
  void SendACKOnIOThread(int routing_id, WebKit::WebInputEvent::Type event_type,
                         InputEventAckState ack_result);

  scoped_refptr<base::MessageLoopProxy> main_loop_;
  IPC::Listener* main_listener_;

  // The sender_ only gets invoked on the thread corresponding to io_loop_.
  scoped_refptr<base::MessageLoopProxy> io_loop_;
  IPC::Sender* sender_;

  // The handler_ only gets Run on the thread corresponding to target_loop_.
  scoped_refptr<base::MessageLoopProxy> target_loop_;
  Handler handler_;

  // Protects access to routes_.
  base::Lock routes_lock_;

  // Indicates the routing_ids for which input events should be filtered.
  std::set<int> routes_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_INPUT_EVENT_FILTER_H_
