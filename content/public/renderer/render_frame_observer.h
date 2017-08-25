// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/web_client_hints_types.mojom.h"
#include "third_party/WebKit/public/platform/web_feature.mojom.h"
#include "third_party/WebKit/public/web/WebMeaningfulLayout.h"
#include "v8/include/v8.h"

namespace blink {
class WebDocumentLoader;
class WebFormElement;
class WebNode;
class WebString;
struct WebURLError;
class WebWorkerFetchContext;
}

namespace content {

class RendererPpapiHost;
class RenderFrame;
class RenderFrameImpl;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderFrameObserver : public IPC::Listener,
                                           public IPC::Sender {
 public:
  // A subclass can use this to delete itself. If it does not, the subclass must
  // always null-check each call to render_frame() becase the RenderFrame can
  // go away at any time.
  virtual void OnDestruct() = 0;

  // Called when a Pepper plugin is created.
  virtual void DidCreatePepperPlugin(RendererPpapiHost* host) {}

  // Called when a load is explicitly stopped by the user or browser.
  virtual void OnStop() {}

  // Called when the RenderFrame visiblity is changed.
  virtual void WasHidden() {}
  virtual void WasShown() {}

  // Called when associated widget is about to close.
  virtual void WidgetWillClose() {}

  // These match the Blink API notifications
  virtual void DidCreateNewDocument() {}
  virtual void DidCreateDocumentElement() {}
  // Called when a provisional load is about to commit in a frame. This is
  // dispatched just before the Javascript unload event.
  virtual void WillCommitProvisionalLoad() {}
  virtual void DidCommitProvisionalLoad(bool is_new_navigation,
                                        bool is_same_document_navigation) {}
  virtual void DidStartProvisionalLoad(
      blink::WebDocumentLoader* document_loader) {}
  virtual void DidFailProvisionalLoad(const blink::WebURLError& error) {}
  virtual void DidFinishLoad() {}
  virtual void DidFinishDocumentLoad() {}
  virtual void DidCreateScriptContext(v8::Local<v8::Context> context,
                                      int world_id) {}
  virtual void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                        int world_id) {}
  virtual void DidClearWindowObject() {}
  virtual void DidChangeManifest() {}
  virtual void DidChangeScrollOffset() {}
  virtual void WillSendSubmitEvent(const blink::WebFormElement& form) {}
  virtual void WillSubmitForm(const blink::WebFormElement& form) {}
  virtual void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors) {}

  // Called when this frame has been detached from the view. This *will* be
  // called for child frames when a parent frame is detached. Since the frame is
  // already detached from the DOM at this time, it should not be inspected.
  virtual void FrameDetached() {}

  // Called when we receive a console message from Blink for which we requested
  // extra details (like the stack trace). |message| is the error message,
  // |source| is the Blink-reported source of the error (either external or
  // internal), and |stack_trace| is the stack trace of the error in a
  // human-readable format (each frame is formatted as
  // "\n    at function_name (source:line_number:column_number)").
  virtual void DetailedConsoleMessageAdded(const base::string16& message,
                                           const base::string16& source,
                                           const base::string16& stack_trace,
                                           uint32_t line_number,
                                           int32_t severity_level) {}

  // Called when an interesting (from document lifecycle perspective),
  // compositor-driven layout had happened. This is a reasonable hook to use
  // to inspect the document and layout information, since it is in a clean
  // state and you won't accidentally force new layouts.
  // The interestingness of layouts is explained in WebMeaningfulLayout.h.
  virtual void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {}

  // Called when a compositor frame has committed.
  virtual void DidCommitCompositorFrame() {}

  // Notifications when |PerformanceTiming| data becomes available
  virtual void DidChangePerformanceTiming() {}

  // Notification when the renderer uses a particular code path during a page
  // load. This is used for metrics collection.
  virtual void DidObserveLoadingBehavior(
      blink::WebLoadingBehaviorFlag behavior) {}

  // Notification when the renderer observes a new feature usage during a page
  // load. This is used for UseCounter feature metrics.
  virtual void DidObserveNewFeatureUsage(blink::mojom::WebFeature feature) {}

  // Called when the focused node has changed to |node|.
  virtual void FocusedNodeChanged(const blink::WebNode& node) {}

  // Called when accessibility is enabled or disabled.
  virtual void AccessibilityModeChanged() {}

  // Called when script in the page calls window.print().
  virtual void ScriptedPrint(bool user_initiated) {}

  // Called when draggable regions change.
  virtual void DraggableRegionsChanged() {}

  // Called when a worker fetch context will be created.
  virtual void WillCreateWorkerFetchContext(blink::WebWorkerFetchContext*) {}

  // Called to give the embedder an opportunity to bind an interface request
  // for a frame. If the request can be bound, |interface_pipe| will be taken.
  virtual void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  RenderFrame* render_frame() const;
  int routing_id() const { return routing_id_; }

 protected:
  explicit RenderFrameObserver(RenderFrame* render_frame);
  ~RenderFrameObserver() override;

 private:
  friend class RenderFrameImpl;

  // This is called by the RenderFrame when it's going away so that this object
  // can null out its pointer.
  void RenderFrameGone();

  RenderFrame* render_frame_;
  // The routing ID of the associated RenderFrame.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
