// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_

#include "android_webview/browser/browser_view_renderer_impl.h"

#include "content/public/renderer/android/synchronous_compositor_client.h"

namespace content {
class SynchronousCompositor;
}

namespace android_webview {

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
class InProcessViewRenderer : public BrowserViewRendererImpl,
                              public content::SynchronousCompositorClient {
 public:
  InProcessViewRenderer(BrowserViewRenderer::Client* client,
                        JavaHelper* java_helper);
  virtual ~InProcessViewRenderer();

  // BrowserViewRenderer overrides
  virtual void BindSynchronousCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual bool RenderPicture(SkCanvas* canvas) OVERRIDE;

  // SynchronousCompositorClient overrides
 virtual void DidDestroyCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;

 private:
  content::SynchronousCompositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(InProcessViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_
