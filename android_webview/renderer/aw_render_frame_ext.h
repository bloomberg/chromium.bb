// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_

#include "content/public/renderer/render_frame_observer.h"

namespace android_webview {

// Render process side of AwRenderViewHostExt, this provides cross-process
// implementation of miscellaneous WebView functions that we need to poke
// WebKit directly to implement (and that aren't needed in the chrome app).
class AwRenderFrameExt : public content::RenderFrameObserver {
 public:
  AwRenderFrameExt(content::RenderFrame* render_frame);

 private:
  virtual ~AwRenderFrameExt();

  // RenderFrame::Observer:
  virtual void DidCommitProvisionalLoad(bool is_new_navigation) OVERRIDE;
  DISALLOW_COPY_AND_ASSIGN(AwRenderFrameExt);
};

}

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_EXT_H_


