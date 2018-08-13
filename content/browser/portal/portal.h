// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PORTAL_PORTAL_H_
#define CONTENT_BROWSER_PORTAL_PORTAL_H_

#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"

namespace content {

class RenderFrameHostImpl;

// A Portal provides a way to embed a WebContents inside a frame in another
// WebContents. It also provides an API that the owning frame can interact with
// the portal WebContents. The portal can be activated, where the portal
// WebContents replaces the outer WebContents and inherit it as a new Portal.
//
// The Portal is owned by its mojo binding, so it is kept alive as long as the
// other end of the pipe (typically in the renderer) exists.
class Portal : public blink::mojom::Portal, public WebContentsObserver {
 public:
  ~Portal() override;

  static bool IsEnabled();
  static Portal* Create(RenderFrameHostImpl* owner_render_frame_host,
                        blink::mojom::PortalRequest request);

  // WebContentsObserver overrides.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

 private:
  explicit Portal(RenderFrameHostImpl* owner_render_frame_host);

  RenderFrameHostImpl* owner_render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PORTAL_PORTAL_H_
