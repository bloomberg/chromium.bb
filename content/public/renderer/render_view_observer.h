// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIconURL.h"

namespace ppapi {
namespace host {
class PpapiHost;
}
}

namespace WebKit {
class WebDataSource;
class WebFrame;
class WebFormElement;
class WebGestureEvent;
class WebMediaPlayerClient;
class WebMouseEvent;
class WebNode;
class WebTouchEvent;
class WebURL;
struct WebContextMenuData;
struct WebURLError;
}

namespace content {

class RendererPpapiHost;
class RenderView;
class RenderViewImpl;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderViewObserver : public IPC::Listener,
                                          public IPC::Sender {
 public:
  // By default, observers will be deleted when the RenderView goes away.  If
  // they want to outlive it, they can override this function.
  virtual void OnDestruct();

  // These match the WebKit API notifications
  virtual void DidStartLoading() {}
  virtual void DidStopLoading() {}
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
  virtual void FrameCreated(WebKit::WebFrame* parent,
                            WebKit::WebFrame* frame) {}
  virtual void FrameDetached(WebKit::WebFrame* frame) {}
  virtual void FrameWillClose(WebKit::WebFrame* frame) {}
  virtual void WillSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form) {}
  virtual void DidCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* ds) {}
  virtual void PrintPage(WebKit::WebFrame* frame, bool user_initiated) {}
  virtual void FocusedNodeChanged(const WebKit::WebNode& node) {}
  virtual void WillCreateMediaPlayer(WebKit::WebFrame* frame,
                                     WebKit::WebMediaPlayerClient* client) {}
  virtual void ZoomLevelChanged() {};
  virtual void DidChangeScrollOffset(WebKit::WebFrame* frame) {}
  virtual void DraggableRegionsChanged(WebKit::WebFrame* frame) {}
  virtual void DidRequestShowContextMenu(
      WebKit::WebFrame* frame,
      const WebKit::WebContextMenuData& data) {}
  virtual void DidActivateCompositor(int input_handler_id) {}
  virtual void DidCommitCompositorFrame() {}

  // These match the RenderView methods.
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {}
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event) {}
  virtual void DidHandleGestureEvent(const WebKit::WebGestureEvent& event) {}
  virtual void DidCreatePepperPlugin(RendererPpapiHost* host) {}

  // These match incoming IPCs.
  virtual void Navigate(const GURL& url) {}
  virtual void ClosePage() {}

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  explicit RenderViewObserver(RenderView* render_view);
  virtual ~RenderViewObserver();

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  RenderView* render_view() const;
  int routing_id() { return routing_id_; }

 private:
  friend class RenderViewImpl;

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
