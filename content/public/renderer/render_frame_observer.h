// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "v8/include/v8.h"

namespace blink {
class WebFrame;
struct WebURLError;
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
  // By default, observers will be deleted when the RenderFrame goes away.  If
  // they want to outlive it, they can override this function.
  virtual void OnDestruct();

  // Called when a Pepper plugin is created.
  virtual void DidCreatePepperPlugin(RendererPpapiHost* host) {}

  // Called when a load is explicitly stopped by the user or browser.
  virtual void OnStop() {}

  // These match the Blink API notifications
  virtual void DidCommitProvisionalLoad(bool is_new_navigation) {}
  virtual void DidStartProvisionalLoad() {}
  virtual void DidFailProvisionalLoad(const blink::WebURLError& error) {}
  virtual void DidFinishLoad() {}
  virtual void WillReleaseScriptContext(v8::Handle<v8::Context> context,
                                        int world_id) {}
  virtual void DidClearWindowObject(int world_id) {}

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  RenderFrame* render_frame() const;
  int routing_id() const { return routing_id_; }

 protected:
  explicit RenderFrameObserver(RenderFrame* render_frame);
  virtual ~RenderFrameObserver();

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
