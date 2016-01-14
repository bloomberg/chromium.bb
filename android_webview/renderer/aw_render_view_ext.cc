// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_view_ext.h"
#include "android_webview/common/render_view_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace android_webview {

AwRenderViewExt::AwRenderViewExt(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {}

AwRenderViewExt::~AwRenderViewExt() {}

// static
void AwRenderViewExt::RenderViewCreated(content::RenderView* render_view) {
  new AwRenderViewExt(render_view);  // |render_view| takes ownership.
}

void AwRenderViewExt::DidCommitCompositorFrame() {
  PostCheckContentsSize();
}

void AwRenderViewExt::DidUpdateLayout() {
  PostCheckContentsSize();
}

void AwRenderViewExt::PostCheckContentsSize() {
  if (check_contents_size_timer_.IsRunning())
    return;

  check_contents_size_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(0), this,
      &AwRenderViewExt::CheckContentsSize);
}

void AwRenderViewExt::CheckContentsSize() {
  blink::WebView* webview = render_view()->GetWebView();
  if (!webview)
    return;

  gfx::Size contents_size;

  blink::WebFrame* main_frame = webview->mainFrame();
  if (main_frame)
    contents_size = main_frame->contentsSize();

  // Fall back to contentsPreferredMinimumSize if the mainFrame is reporting a
  // 0x0 size (this happens during initial load).
  if (contents_size.IsEmpty()) {
    contents_size = webview->contentsPreferredMinimumSize();
  }

  if (contents_size == last_sent_contents_size_)
    return;

  last_sent_contents_size_ = contents_size;
  render_view()->GetMainRenderFrame()->Send(
      new AwViewHostMsg_OnContentsSizeChanged(
        render_view()->GetMainRenderFrame()->GetRoutingID(),
        contents_size));
}

void AwRenderViewExt::Navigate(const GURL& url) {
  // Navigate is called only on NEW navigations, so WebImageCache won't be
  // freed when the user just clicks on links, but only when a navigation is
  // started, for instance via loadUrl. A better approach would be clearing the
  // cache on cross-site boundaries, however this would require too many
  // changes both on the browser side (in RenderViewHostManger), to the
  // IPCmessages and to the RenderViewObserver. Thus, clearing decoding image
  // cache on Navigate, seems a more acceptable compromise.
  blink::WebImageCache::clear();
}

}  // namespace android_webview
