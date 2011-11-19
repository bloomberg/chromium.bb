// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_THREAD_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_THREAD_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "ipc/ipc_channel_proxy.h"
#include "webkit/glue/webthread_impl.h"

namespace WebKit {
class WebInputEvent;
}

class InputEventFilter;

// The CompositorThread class manages the background thread for the compositor.
// The CompositorThread instance can be assumed to outlive the background
// thread it manages.
class CompositorThread {
 public:
  // |main_listener| refers to the central IPC message listener that lives on
  // the main thread, where all incoming IPC messages are first handled.
  explicit CompositorThread(IPC::Channel::Listener* main_listener);
  ~CompositorThread();

  // This MessageFilter should be added to allow input events to be redirected
  // to the compositor's thread.
  IPC::ChannelProxy::MessageFilter* GetMessageFilter() const;

  // Callable from the main thread or the compositor's thread.
  void AddCompositor(int routing_id, int compositor_id);

  webkit_glue::WebThreadImpl* GetWebThread() { return &thread_; }

 private:
  // Callback only from the compositor's thread.
  void RemoveCompositor(int routing_id);

  // Called from the compositor's thread.
  void HandleInputEvent(int routing_id,
                        const WebKit::WebInputEvent* input_event);

  class CompositorWrapper;
  friend class CompositorWrapper;

  typedef std::map<int,  // routing_id
                   linked_ptr<CompositorWrapper> > CompositorMap;
  CompositorMap compositors_;

  webkit_glue::WebThreadImpl thread_;
  scoped_refptr<InputEventFilter> filter_;
};

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_THREAD_H_
