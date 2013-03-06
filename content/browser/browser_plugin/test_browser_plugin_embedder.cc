// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_embedder.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

TestBrowserPluginEmbedder::TestBrowserPluginEmbedder(
    WebContentsImpl* web_contents)
    : BrowserPluginEmbedder(web_contents),
      last_rvh_at_position_response_(NULL) {
}

TestBrowserPluginEmbedder::~TestBrowserPluginEmbedder() {
}

void TestBrowserPluginEmbedder::GetRenderViewHostCallback(
    RenderViewHost* rvh, int x, int y) {
  last_rvh_at_position_response_ = rvh;
  if (message_loop_runner_)
    message_loop_runner_->Quit();
}

void TestBrowserPluginEmbedder::WaitForRenderViewHostAtPosition(int x, int y) {
  GetRenderViewHostAtPosition(x, y,
      base::Bind(&TestBrowserPluginEmbedder::GetRenderViewHostCallback,
                 base::Unretained(this)));
  message_loop_runner_ = new MessageLoopRunner();
  message_loop_runner_->Run();
}

WebContentsImpl* TestBrowserPluginEmbedder::web_contents() const {
  return static_cast<WebContentsImpl*>(BrowserPluginEmbedder::web_contents());
}

}  // namespace content
