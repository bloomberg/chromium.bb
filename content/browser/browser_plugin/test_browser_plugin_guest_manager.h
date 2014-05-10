// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_MANAGER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_MANAGER_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"
#include "content/public/test/test_utils.h"

FORWARD_DECLARE_TEST(BrowserPluginHostTest, ReloadEmbedder);

namespace content {

class WebContentsImpl;

// Test class for BrowserPluginGuestManager.
//
// Provides utilities to wait for certain state/messages in
// BrowserPluginGuestManager to be used in tests.
class TestBrowserPluginGuestManager : public BrowserPluginGuestManager {
 public:
  explicit TestBrowserPluginGuestManager(BrowserContext* context);
  virtual ~TestBrowserPluginGuestManager();

  // Overriden to intercept in test.
  virtual BrowserPluginGuest* CreateGuest(
      SiteInstance* embedder_site_instance,
      int instance_id,
      const std::string& storage_partition_id,
      bool persist_storage,
      scoped_ptr<base::DictionaryValue> extra_params) OVERRIDE;

  // Waits until at least one guest is added to the guest manager.
  WebContents* WaitForGuestAdded();

 private:
  // BrowserPluginHostTest.ReloadEmbedder needs access to the GuestInstanceMap.
  FRIEND_TEST_ALL_PREFIXES(BrowserPluginHostTest, ReloadEmbedder);

  WebContents* last_guest_added_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginGuestManager);
};

} // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_TEST_BROWSER_PLUGIN_GUEST_MANAGER_H_
