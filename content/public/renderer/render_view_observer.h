// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIconURL.h"

class RenderViewImpl;

namespace WebKit {
class WebDataSource;
class WebFrame;
class WebFormElement;
class WebMediaPlayerClient;
class WebMouseEvent;
class WebNode;
class WebTouchEvent;
class WebURL;
struct WebURLError;
}

namespace content {

class RenderView;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderViewObserver : public IPC::Channel::Listener,
                                          public IPC::Message::Sender {
 public:
  // By default, observers will be deleted when the RenderView goes away.  If
  // they want to outlive it, they can override this function.
  virtual void OnDestruct();

  // These match the WebKit API notifications
  virtual void DidStartLoading() {}
  virtual void DidStopLoading() {}
  virtual void DidChangeIcon(WebKit::WebFrame* frame,
                             WebKit::WebIconURL::Type) {}
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) {}
  virtual void DidFailLoad(WebKit::WebFrame* frame,
                           const WebKit::WebURLError& error) {}
  virtual void DidFinishLoad(WebKit::WebFrame* frame) {}
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame) {}
  virtual void DidFailProvisionalLoad(WebKit::WebFrame* frame,
                                      const WebKit::WebURLError& error) {}
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) {}
  virtual void DidClearWindowObject(WebKit::WebFrame* frame) {}
  virtual void WillPerformClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from,
      const WebKit::WebURL& to, double interval, double fire_time) {}
  virtual void DidCancelClientRedirect(WebKit::WebFrame* frame) {}
  virtual void DidCompleteClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from) {}
  virtual void DidCreateDocumentElement(WebKit::WebFrame* frame) {}
  virtual void FrameDetached(WebKit::WebFrame* frame) {}
  virtual void FrameWillClose(WebKit::WebFrame* frame) {}
  virtual void WillSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form) {}
  virtual void DidCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* ds) {}
  virtual void PrintPage(WebKit::WebFrame* frame) {}
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) {}
  virtual void WillCreateMediaPlayer(WebKit::WebFrame* frame,
                                     WebKit::WebMediaPlayerClient* client) {}
  virtual void ZoomLevelChanged() {};
  virtual void DidChangeScrollOffset(WebKit::WebFrame* frame) {}

  // These match the RenderView methods.
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {}
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event) {}

  // These match incoming IPCs.
  virtual void ContextMenuAction(unsigned id) {}
  virtual void Navigate(const GURL& url) {}
  virtual void ClosePage() {}

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  explicit RenderViewObserver(RenderView* render_view);
  virtual ~RenderViewObserver();

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  RenderView* render_view();
  int routing_id() { return routing_id_; }

 private:
  friend class ::RenderViewImpl;

  // This is called by the RenderView when it's going away so that this object
  // can null out its pointer.
  void RenderViewGone();

  RenderView* render_view_;
  // The routing ID of the associated RenderView.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_
