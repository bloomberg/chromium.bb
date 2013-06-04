// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_INPUT_HANDLER_MANAGER_H_
#define CONTENT_RENDERER_GPU_INPUT_HANDLER_MANAGER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

namespace cc {
class InputHandler;
}

namespace WebKit {
class WebInputEvent;
}

namespace content {

class InputEventFilter;
class InputHandlerWrapper;

// InputHandlerManager class manages InputHandlerProxy instances for
// the WebViews in this renderer.
class InputHandlerManager {
 public:
  // |main_listener| refers to the central IPC message listener that lives on
  // the main thread, where all incoming IPC messages are first handled.
  // |message_loop_proxy| is the MessageLoopProxy of the compositor thread.
  // The underlying MessageLoop must outlive this object.
  InputHandlerManager(
      IPC::Listener* main_listener,
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy);
  ~InputHandlerManager();

  // This MessageFilter should be added to allow input events to be redirected
  // to the compositor's thread.
  IPC::ChannelProxy::MessageFilter* GetMessageFilter() const;

  // Callable from the main thread only.
  void AddInputHandler(
      int routing_id,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderViewImpl>& render_view_impl);

  // Callback only from the compositor's thread.
  void RemoveInputHandler(int routing_id);

  // Called from the compositor's thread.
  InputEventAckState HandleInputEvent(int routing_id,
                                      const WebKit::WebInputEvent* input_event);

  // Called from the compositor's thread.
  InputEventFilter* filter() { return filter_.get(); }

 private:
  // Called from the compositor's thread.
  void AddInputHandlerOnCompositorThread(
      int routing_id,
      const scoped_refptr<base::MessageLoopProxy>& main_loop,
      const base::WeakPtr<cc::InputHandler>& input_handler,
      const base::WeakPtr<RenderViewImpl>& render_view_impl);

  typedef std::map<int,  // routing_id
                   scoped_refptr<InputHandlerWrapper> > InputHandlerMap;
  InputHandlerMap input_handlers_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<InputEventFilter> filter_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_INPUT_HANDLER_MANAGER_H_
