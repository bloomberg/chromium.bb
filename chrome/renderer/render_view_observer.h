// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_VIEW_OBSERVER_H_
#define CHROME_RENDERER_RENDER_VIEW_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "ipc/ipc_channel.h"

class RenderView;

namespace WebKit {
class WebFrame;
class WebMouseEvent;
struct WebURLError;
}

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class RenderViewObserver : public IPC::Channel::Listener,
                           public IPC::Message::Sender {
 public:
  // By default, observers will be deleted when the RenderView goes away.  If
  // they want to outlive it, they can override this function.
  virtual void OnDestruct();

  // These match the WebKit API notifications.
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) {}
  virtual void DidFailLoad(WebKit::WebFrame* frame,
                           const WebKit::WebURLError& error) {}
  virtual void DidFinishLoad(WebKit::WebFrame* frame) {}
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame) {}
  virtual void DidFailProvisionalLoad(WebKit::WebFrame* frame,
                                      const WebKit::WebURLError& error) {}
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) {}
  virtual void FrameDetached(WebKit::WebFrame* frame) {}
  virtual void FrameWillClose(WebKit::WebFrame* frame) {}

  // These match the RenderView methods below.
  virtual void FrameTranslated(WebKit::WebFrame* frame) {}
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {}
  virtual void PageCaptured(const string16& page_text) {}

 protected:
  RenderViewObserver(RenderView* render_view);
  virtual ~RenderViewObserver();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message);

  RenderView* render_view() { return render_view_; }
  int routing_id() { return routing_id_; }

 private:
  friend class RenderView;

  void set_render_view(RenderView* rv) { render_view_ = rv; }

  RenderView* render_view_;
  // The routing ID of the associated RenderView.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewObserver);
};

#endif  // CHROME_RENDERER_RENDER_VIEW_OBSERVER_H_
