// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest_manager.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_utils.h"

namespace content {

TestBrowserPluginGuestManager::TestBrowserPluginGuestManager(
    BrowserContext* context)
    : BrowserPluginGuestManager(context),
      last_guest_added_(NULL) {
}

TestBrowserPluginGuestManager::~TestBrowserPluginGuestManager() {
}

void TestBrowserPluginGuestManager::AddGuest(
    int instance_id,
    WebContents* guest_web_contents) {
  BrowserPluginGuestManager::AddGuest(instance_id, guest_web_contents);
  last_guest_added_ = guest_web_contents;
  if (message_loop_runner_.get())
    message_loop_runner_->Quit();
}

WebContents* TestBrowserPluginGuestManager::WaitForGuestAdded() {
  // Check if guests were already created.
  if (last_guest_added_) {
    WebContents* last_guest_added = last_guest_added_;
    last_guest_added_ = NULL;
    return last_guest_added;
  }
  // Wait otherwise.
  message_loop_runner_ = new MessageLoopRunner();
  message_loop_runner_->Run();
  WebContents* last_guest_added = last_guest_added_;
  last_guest_added_ = NULL;
  return last_guest_added;
}

}  // namespace content
