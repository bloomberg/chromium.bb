// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_OBSERVER_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace android_webview {

// Android WebView implementation of blink::WebPermissionClient.
class AwRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit AwRenderFrameObserver(content::RenderFrame* render_view);

 private:
  virtual ~AwRenderFrameObserver();

  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC handlers
  void OnPrintNodeUnderContextMenu();

  DISALLOW_COPY_AND_ASSIGN(AwRenderFrameObserver);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_FRAME_OBSERVER_H_
