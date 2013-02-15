// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_RENDERER_VIEW_RENDERER_H_

#include "content/public/renderer/render_view_observer.h"

namespace android_webview {

// Render-process side of ViewRendererHost.
// Implements required interaction with content::RenderView.
class ViewRenderer : public content::RenderViewObserver {
 public:
  static void RenderViewCreated(content::RenderView* render_view);

  // Required to be public by IPC_MESSAGE_HANDLER.
  using content::RenderViewObserver::Send;

 private:
  ViewRenderer(content::RenderView* render_view);
  virtual ~ViewRenderer();

  // content::RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCommitCompositorFrame() OVERRIDE;

  void OnEnableCapturePictureCallback(bool enable);
  void OnCapturePictureSync();

  bool capture_picture_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_VIEW_RENDERER_H_
