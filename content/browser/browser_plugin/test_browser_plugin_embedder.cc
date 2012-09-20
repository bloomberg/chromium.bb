// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_embedder.h"

#include "base/time.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_utils.h"

namespace content {

TestBrowserPluginEmbedder::TestBrowserPluginEmbedder(
    WebContentsImpl* web_contents,
    RenderViewHost* render_view_host)
    : BrowserPluginEmbedder(web_contents, render_view_host) {
}

TestBrowserPluginEmbedder::~TestBrowserPluginEmbedder() {
}

void TestBrowserPluginEmbedder::AddGuest(int instance_id,
                                         WebContents* guest_web_contents,
                                         int64 frame_id) {
  BrowserPluginEmbedder::AddGuest(instance_id, guest_web_contents, frame_id);
  if (message_loop_runner_)
    message_loop_runner_->Quit();
}

void TestBrowserPluginEmbedder::WaitForGuestAdded() {
  // Check if guests were already created.
  if (guest_web_contents_by_instance_id_.size() > 0)
    return;
  // Wait otherwise.
  message_loop_runner_ = new MessageLoopRunner();
  message_loop_runner_->Run();
}

}  // namespace content
