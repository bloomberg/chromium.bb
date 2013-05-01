// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/in_process_renderer/in_process_renderer_client.h"

#include "android_webview/browser/browser_view_renderer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace android_webview {

namespace {

int GetInProcessRendererId() {
  content::RenderProcessHost::iterator it =
    content::RenderProcessHost::AllHostsIterator();
  if (it.IsAtEnd()) {
    // There should always be one RPH in single process more.
    NOTREACHED();
    return 0;
  }

  int id = it.GetCurrentValue()->GetID();
  it.Advance();
  DCHECK(it.IsAtEnd());  // Not multiprocess compatible.
  return id;
}

}

MessageLoop* InProcessRendererClient::OverrideCompositorMessageLoop() const {
 MessageLoop* rv = content::BrowserThread::UnsafeGetMessageLoopForThread(
      content::BrowserThread::UI);
  DCHECK(rv);
  return rv;
}

void InProcessRendererClient::DidCreateSynchronousCompositor(
    int render_view_id,
    content::SynchronousCompositor* compositor) {
  BrowserViewRendererImpl* view_renderer =
      BrowserViewRendererImpl::FromId(GetInProcessRendererId(), render_view_id);
  if (view_renderer)
    view_renderer->BindSynchronousCompositor(compositor);
}

bool InProcessRendererClient::ShouldCreateCompositorInputHandler() const {
  // Compositor input handling will be performed by the renderer host
  // when UI and compositor threads are merged, so we disable client compositor
  // input handling in this case.
  return false;
}

}  // namespace android_webview
