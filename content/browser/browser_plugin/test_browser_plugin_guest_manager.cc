// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/test_browser_plugin_guest_manager.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
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

BrowserPluginGuest* TestBrowserPluginGuestManager::CreateGuest(
    SiteInstance* embedder_site_instance,
    int instance_id,
    const std::string& storage_partition_id,
    bool persist_storage,
    scoped_ptr<base::DictionaryValue> extra_params) {
  BrowserPluginGuest* guest = BrowserPluginGuestManager::CreateGuest(
      embedder_site_instance,
      instance_id,
      storage_partition_id,
      persist_storage,
      extra_params.Pass());
  last_guest_added_ = guest->GetWebContents();
  if (message_loop_runner_.get())
    message_loop_runner_->Quit();
  return guest;
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
