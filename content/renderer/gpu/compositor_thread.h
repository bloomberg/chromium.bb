// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_THREAD_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_THREAD_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_channel_proxy.h"
#include "webkit/glue/webthread_impl.h"

namespace WebKit {
class WebInputEvent;
}

namespace content {

class InputEventFilter;

// The CompositorThread class manages the background thread for the compositor.
// The CompositorThread instance can be assumed to outlive the background
// thread it manages.
class CompositorThread {
 public:
  // |main_listener| refers to the central IPC message listener that lives on
  // the main thread, where all incoming IPC messages are first handled.
  explicit CompositorThread(IPC::Listener* main_listener);
  ~CompositorThread();

  // This MessageFilter should be added to allow input events to be redirected
  // to the compositor's thread.
  IPC::ChannelProxy::MessageFilter* GetMessageFilter() const;

  // Callable from the main thread only.
  void AddInputHandler(int routing_id,
                       int input_handler_id,
                       const base::WeakPtr<RenderViewImpl>& render_view_impl);

  webkit_glue::WebThreadImpl* GetWebThread() { return &thread_; }

  MessageLoop* message_loop() { return thread_.message_loop(); }

 private:
  // Callback only from the compositor's thread.
  void RemoveInputHandler(int routing_id);

  // Called from the compositor's thread.
  void HandleInputEvent(int routing_id,
                        const WebKit::WebInputEvent* input_event);

  // Called from the compositor's thread.
  void AddInputHandlerOnCompositorThread(
      int routing_id,
      int input_handler_id,
      scoped_refptr<base::MessageLoopProxy> main_loop,
      const base::WeakPtr<RenderViewImpl>& render_view_impl);

  class InputHandlerWrapper;
  friend class InputHandlerWrapper;

  typedef std::map<int,  // routing_id
                   scoped_refptr<InputHandlerWrapper> > InputHandlerMap;
  InputHandlerMap input_handlers_;

  webkit_glue::WebThreadImpl thread_;
  scoped_refptr<InputEventFilter> filter_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_THREAD_H_
