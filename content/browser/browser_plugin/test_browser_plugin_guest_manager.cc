// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest_manager.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_utils.h"

namespace content {

TestBrowserPluginGuestManager::TestBrowserPluginGuestManager() {
}

TestBrowserPluginGuestManager::~TestBrowserPluginGuestManager() {
}

void TestBrowserPluginGuestManager::AddGuest(
    int instance_id,
    WebContentsImpl* guest_web_contents) {
  BrowserPluginGuestManager::AddGuest(instance_id, guest_web_contents);
  if (message_loop_runner_)
    message_loop_runner_->Quit();
}

void TestBrowserPluginGuestManager::WaitForGuestAdded() {
  // Check if guests were already created.
  if (guest_web_contents_by_instance_id_.size() > 0)
    return;
  // Wait otherwise.
  message_loop_runner_ = new MessageLoopRunner();
  message_loop_runner_->Run();
}

}  // namespace content
