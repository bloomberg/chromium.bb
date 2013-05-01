// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/in_process_renderer/in_process_view_renderer.h"

#include "base/logging.h"
#include "content/public/renderer/android/synchronous_compositor.h"

namespace android_webview {

InProcessViewRenderer::InProcessViewRenderer(
    BrowserViewRenderer::Client* client,
    JavaHelper* java_helper)
    : BrowserViewRendererImpl(client, java_helper),
      compositor_(NULL) {
}

InProcessViewRenderer::~InProcessViewRenderer() {
  if (compositor_)
    compositor_->SetClient(NULL);
}

void InProcessViewRenderer::BindSynchronousCompositor(
    content::SynchronousCompositor* compositor) {
  DCHECK(compositor && compositor_ != compositor);
  if (compositor_)
    compositor_->SetClient(NULL);
  compositor_ = compositor;
  compositor_->SetClient(this);
}

bool InProcessViewRenderer::RenderPicture(SkCanvas* canvas) {
  return compositor_ && compositor_->DemandDrawSw(canvas);
}

void InProcessViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  // Allow for transient hand-over when two compositors may reference
  // a single client.
  if (compositor_ == compositor)
    compositor_ = NULL;
}

}  // namespace android_webview
