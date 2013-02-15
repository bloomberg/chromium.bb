// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/view_renderer.h"

#include "android_webview/common/render_view_messages.h"
#include "android_webview/common/renderer_picture_map.h"
#include "content/public/renderer/render_view.h"
#include "skia/ext/refptr.h"

namespace android_webview {

// static
void ViewRenderer::RenderViewCreated(content::RenderView* render_view) {
  new ViewRenderer(render_view);  // |render_view| takes ownership.
}

ViewRenderer::ViewRenderer(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      // TODO(leandrogracia): default to false when SW mode uses Ubercompositor.
      // Until then we need picture updates enabled for SW mode invalidation.
      // http://crbug.com/170086.
      capture_picture_enabled_(true) {
}

ViewRenderer::~ViewRenderer() {
  RendererPictureMap::GetInstance()->ClearRendererPicture(routing_id());
}

bool ViewRenderer::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ViewRenderer, message)
    IPC_MESSAGE_HANDLER(AwViewMsg_EnableCapturePictureCallback,
                        OnEnableCapturePictureCallback)
    IPC_MESSAGE_HANDLER(AwViewMsg_CapturePictureSync,
                        OnCapturePictureSync)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ViewRenderer::DidCommitCompositorFrame() {
  if (!capture_picture_enabled_)
    return;

  skia::RefPtr<SkPicture> picture = render_view()->CapturePicture();
  RendererPictureMap::GetInstance()->SetRendererPicture(routing_id(), picture);
  Send(new AwViewHostMsg_PictureUpdated(routing_id()));
}

void ViewRenderer::OnEnableCapturePictureCallback(bool enable) {
  capture_picture_enabled_ = enable;
}

void ViewRenderer::OnCapturePictureSync() {
  RendererPictureMap::GetInstance()->SetRendererPicture(
      routing_id(), render_view()->CapturePicture());
}

}  // namespace android_webview
