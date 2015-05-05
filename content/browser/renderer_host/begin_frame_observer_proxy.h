// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BEGIN_FRAME_OBSERVER_PROXY_H_
#define CONTENT_BROWSER_RENDERER_HOST_BEGIN_FRAME_OBSERVER_PROXY_H_

#include "content/common/content_export.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"

namespace cc {
struct BeginFrameArgs;
}

namespace content {

// This is the interface from the BeginFrameObserverProxy which manages sending
// BeginFrame messages.
class BeginFrameObserverProxyClient {
 public:
  virtual void SendBeginFrame(const cc::BeginFrameArgs& args) = 0;
};

// This class is used to manage all of the RenderWidgetHostView state and
// functionality that is associated with BeginFrame message handling.
class CONTENT_EXPORT BeginFrameObserverProxy
    : public ui::CompositorBeginFrameObserver {
 public:
  explicit BeginFrameObserverProxy(BeginFrameObserverProxyClient* client);
  ~BeginFrameObserverProxy() override;

  void SetNeedsBeginFrames(bool needs_begin_frames);

  void SetCompositor(ui::Compositor* compositor);
  void ResetCompositor();

  // Overridden from ui::CompositorBeginFrameObserver:
  void OnSendBeginFrame(const cc::BeginFrameArgs& args) override;

 private:
  void StartObservingBeginFrames();
  void StopObservingBeginFrames();

  // True when RenderWidget needs a BeginFrame message.
  bool needs_begin_frames_;

  // Used whether to send begin frame to client or not. When |args| from
  // Compositor is different from this, send to client.
  cc::BeginFrameArgs last_sent_begin_frame_args_;

  BeginFrameObserverProxyClient* client_;
  ui::Compositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(BeginFrameObserverProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BEGIN_FRAME_OBSERVER_PROXY_H_
