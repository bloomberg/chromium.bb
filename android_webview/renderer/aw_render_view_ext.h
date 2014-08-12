// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace blink {

class WebNode;
class WebURL;

}  // namespace blink

namespace android_webview {

// Render process side of AwRenderViewHostExt, this provides cross-process
// implementation of miscellaneous WebView functions that we need to poke
// WebKit directly to implement (and that aren't needed in the chrome app).
class AwRenderViewExt : public content::RenderViewObserver {
 public:
  static void RenderViewCreated(content::RenderView* render_view);

 private:
  AwRenderViewExt(content::RenderView* render_view);
  virtual ~AwRenderViewExt();

  // RenderView::Observer:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void FocusedNodeChanged(const blink::WebNode& node) OVERRIDE;
  virtual void DidCommitCompositorFrame() OVERRIDE;
  virtual void DidUpdateLayout() OVERRIDE;
  virtual void Navigate(const GURL& url) OVERRIDE;

  void OnDocumentHasImagesRequest(int id);

  void OnDoHitTest(int view_x, int view_y);

  void OnSetTextZoomFactor(float zoom_factor);

  void OnResetScrollAndScaleState();

  void OnSetInitialPageScale(double page_scale_factor);

  void OnSetBackgroundColor(SkColor c);

  void UpdatePageScaleFactor();

  void CheckContentsSize();

  bool capture_picture_enabled_;

  float page_scale_factor_;

  gfx::Size last_sent_contents_size_;
  base::OneShotTimer<AwRenderViewExt> check_contents_size_timer_;

  DISALLOW_COPY_AND_ASSIGN(AwRenderViewExt);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
