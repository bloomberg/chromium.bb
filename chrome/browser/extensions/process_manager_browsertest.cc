// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

// Exists as a browser test because ExtensionHosts are hard to create without
// a real browser.
typedef ExtensionBrowserTest ProcessManagerBrowserTest;

// Test that basic extension loading creates the appropriate ExtensionHosts
// and background pages.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       ExtensionHostCreation) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllFrames().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  // Process manager gains a background host.
  EXPECT_EQ(1u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(extension.get()));

  // Unload the extension.
  UnloadExtension(extension->id());

  // Background host disappears.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(extension.get()));
}

// Test that loading an extension with a browser action does not create a
// background page and that clicking on the action creates the appropriate
// ExtensionHost.
// Disabled due to flake, see http://crbug.com/315242
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       DISABLED_PopupHostCreation) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // Load an extension with the ability to open a popup but no background
  // page.
  scoped_refptr<const Extension> popup =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("popup"));
  ASSERT_TRUE(popup.get());

  // No background host was added.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(popup.get()));

  // Simulate clicking on the action to open a popup.
  BrowserActionTestUtil test_util(browser());
  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  // Open popup in the first extension.
  test_util.Press(0);
  frame_observer.Wait();
  ASSERT_TRUE(test_util.HasPopup());

  // We now have a view, but still no background hosts.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(popup.get()));
}

// Content loaded from http://hlogonemlfkgpejgnedahbkiabcdhnnn should not
// interact with an installed extension with that ID. Regression test
// for bug 357382.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, HttpHostMatchingExtensionId) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllFrames().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));

  // Set up a test server running at http://[extension-id]
  ASSERT_TRUE(extension.get());
  const std::string& aliased_host = extension->id();
  host_resolver()->AddRule(aliased_host, "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url =
      embedded_test_server()->GetURL("/extensions/test_file_with_body.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr(aliased_host);
  url = url.ReplaceComponents(replace_host);

  // Load a page from the test host in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      url,
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Sanity check that there's no bleeding between the extension and the tab.
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, tab_web_contents->GetVisibleURL());
  EXPECT_FALSE(pm->GetExtensionForWebContents(tab_web_contents))
      << "Non-extension content must not have an associated extension";
  ASSERT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  content::WebContents* extension_web_contents =
      content::WebContents::FromRenderFrameHost(
          *pm->GetRenderFrameHostsForExtension(extension->id()).begin());
  EXPECT_TRUE(extension_web_contents->GetSiteInstance() !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()) !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
}

// Verify correct keepalive count behavior on network request events.
// Regression test for http://crbug.com/535716.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, KeepaliveOnNetworkRequest) {
  // Load an extension with a lazy background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("lazy_background_page")
                        .AppendASCII("broadcast_event"));
  ASSERT_TRUE(extension.get());

  ProcessManager* pm = ProcessManager::Get(profile());
  ProcessManager::FrameSet frames =
      pm->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());

  // Keepalive count at this point is unpredictable as there may be an
  // outstanding event dispatch. We use the current keepalive count as a
  // reliable baseline for future expectations.
  int baseline_keepalive = pm->GetLazyKeepaliveCount(extension.get());

  // Simulate some network events. This test assumes no other network requests
  // are pending, i.e., that there are no conflicts with the fake request IDs
  // we're using. This should be a safe assumption because LoadExtension should
  // wait for loads to complete, and we don't run the message loop otherwise.
  content::RenderFrameHost* frame_host = *frames.begin();
  pm->OnNetworkRequestStarted(frame_host, 1);
  EXPECT_EQ(baseline_keepalive + 1, pm->GetLazyKeepaliveCount(extension.get()));
  pm->OnNetworkRequestDone(frame_host, 1);
  EXPECT_EQ(baseline_keepalive, pm->GetLazyKeepaliveCount(extension.get()));

  // Simulate only a request completion for this ID and ensure it doesn't result
  // in keepalive decrement.
  pm->OnNetworkRequestDone(frame_host, 2);
  EXPECT_EQ(baseline_keepalive, pm->GetLazyKeepaliveCount(extension.get()));
}

}  // namespace extensions
