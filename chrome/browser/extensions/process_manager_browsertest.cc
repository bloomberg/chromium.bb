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
#include "extensions/browser/extension_system.h"
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
  ProcessManager* pm = ExtensionSystem::Get(profile())->process_manager();

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllViews().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  // Process manager gains a background host.
  EXPECT_EQ(1u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllViews().size());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(1u, pm->GetRenderViewHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(extension.get()));

  // Unload the extension.
  UnloadExtension(extension->id());

  // Background host disappears.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllViews().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(0u, pm->GetRenderViewHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(extension.get()));
}

// Test that loading an extension with a browser action does not create a
// background page and that clicking on the action creates the appropriate
// ExtensionHost.
// Disabled due to flake, see http://crbug.com/315242
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       DISABLED_PopupHostCreation) {
  ProcessManager* pm = ExtensionSystem::Get(profile())->process_manager();

  // Load an extension with the ability to open a popup but no background
  // page.
  scoped_refptr<const Extension> popup =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("popup"));
  ASSERT_TRUE(popup.get());

  // No background host was added.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllViews().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(0u, pm->GetRenderViewHostsForExtension(popup->id()).size());
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
  EXPECT_EQ(1u, pm->GetAllViews().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(1u, pm->GetRenderViewHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(popup.get()));
}

// Content loaded from http://hlogonemlfkgpejgnedahbkiabcdhnnn should not
// interact with an installed extension with that ID. Regression test
// for bug 357382.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, HttpHostMatchingExtensionId) {
  ProcessManager* pm = ExtensionSystem::Get(profile())->process_manager();

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllViews().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));

  // Set up a test server running at http://[extension-id]
  ASSERT_TRUE(extension.get());
  const std::string aliased_host = extension->id();
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
  EXPECT_TRUE(NULL == pm->GetExtensionForRenderViewHost(
                          tab_web_contents->GetRenderViewHost()))
      << "Non-extension content must not have an associated extension";
  ASSERT_EQ(1u, pm->GetRenderViewHostsForExtension(extension->id()).size());
  content::WebContents* extension_web_contents =
      content::WebContents::FromRenderViewHost(
          *pm->GetRenderViewHostsForExtension(extension->id()).begin());
  EXPECT_TRUE(extension_web_contents->GetSiteInstance() !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()) !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
}

}  // namespace extensions
