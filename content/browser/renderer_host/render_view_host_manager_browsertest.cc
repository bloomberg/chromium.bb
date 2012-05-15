// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

using content::RenderViewHost;
using content::SiteInstance;

class RenderViewHostManagerTest : public InProcessBrowserTest {
 public:
  RenderViewHostManagerTest() {
    EnableDOMAutomation();
  }

  static bool GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    std::vector<net::TestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    return net::TestServer::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }
};

// Web pages should not have script access to the swapped out page.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, NoScriptAccessAfterSwapOut) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Open a same-site link in a new tab.
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  new_tab_observer.Wait();

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  EXPECT_EQ("/files/navigate_opener.html",
            browser()->GetSelectedWebContents()->GetURL().path());
  EXPECT_EQ(1, browser()->active_index());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // We should have access to the opened tab's location.
  browser()->ActivateTabAt(0, true);
  success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new tab to a different site.
  browser()->ActivateTabAt(1, true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // We should no longer have script access to the opened tab's location.
  browser()->ActivateTabAt(0, true);
  success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_FALSE(success);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwapProcessWithRelNoreferrerAndTargetBlank) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the tab to open.
  if (browser()->tab_count() < 2)
    ui_test_utils::WaitForNewTab(browser());

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedWebContents()->GetURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      browser()->GetSelectedWebContents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// As of crbug.com/69267, we create a new BrowsingInstance (and SiteInstance)
// for rel=noreferrer links in new windows, even to same site pages and named
// targets.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwapProcessWithSameSiteRelNoreferrer) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a same-site rel=noreferrer + target=foo link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteNoRefTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the tab to open.
  if (browser()->tab_count() < 2)
    ui_test_utils::WaitForNewTab(browser());

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedWebContents()->GetURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      browser()->GetSelectedWebContents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with just
// target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontSwapProcessWithOnlyTargetBlank) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the tab to open.
  if (browser()->tab_count() < 2)
    ui_test_utils::WaitForNewTab(browser());

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Wait for the cross-site transition in the new tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedWebContents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and no target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontSwapProcessWithOnlyRelNoreferrer) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());

  // Opens in same tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedWebContents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for crbug.com/116192.  Targeted links should still work after the
// named target window has swapped processes.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       AllowTargetedNavigationsAfterSwap) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=foo link.
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  new_tab_observer.Wait();

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  EXPECT_EQ("/files/navigate_opener.html",
            browser()->GetSelectedWebContents()->GetURL().path());
  EXPECT_EQ(1, browser()->active_index());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  content::WebContents* new_contents = browser()->GetSelectedWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Clicking the original link in the first tab should cause us to swap back.
  browser()->ActivateTabAt(0, true);
  ui_test_utils::WindowedNotificationObserver navigation_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &new_contents->GetController()));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back and shown the new tab again.
  EXPECT_EQ(1, browser()->active_index());
  scoped_refptr<SiteInstance> revisit_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);

  // If it navigates away to another process, the original window should
  // still be able to close it (using a cross-process close message).
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title1.html"));
  EXPECT_EQ(new_site_instance,
            browser()->GetSelectedWebContents()->GetSiteInstance());
  browser()->ActivateTabAt(0, true);
  ui_test_utils::WindowedNotificationObserver close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(new_contents));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(testCloseWindow());",
      &success));
  EXPECT_TRUE(success);
  close_observer.Wait();
}

// Test for crbug.com/99202.  PostMessage calls should still work after
// navigating the source and target windows to different sites.
// Specifically:
// 1) Create 3 windows (opener, "foo", and _blank) and send "foo" cross-process.
// 2) Fail to post a message from "foo" to opener with the wrong target origin.
// 3) Post a message from "foo" to opener, which replies back to "foo".
// 4) Post a message from _blank to "foo".
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SupportCrossProcessPostMessage) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance and RVHM for later comparison.
  content::WebContents* opener_contents = browser()->GetSelectedWebContents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);
  RenderViewHostManager* opener_manager =
      static_cast<WebContentsImpl*>(opener_contents)->
          GetRenderManagerForTesting();

  // 1) Open two more windows, one named.  These initially have openers but no
  // reference to each other.  We will later post a message between them.

  // First, a named target=foo window.
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      opener_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  new_tab_observer.Wait();

  // Wait for the navigation in the new tab to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  content::WebContents* foo_contents = browser()->GetSelectedWebContents();
  ui_test_utils::WaitForLoadStop(foo_contents);
  EXPECT_EQ("/files/navigate_opener.html", foo_contents->GetURL().path());
  EXPECT_EQ(1, browser()->active_index());
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // Second, a target=_blank window.
  browser()->ActivateTabAt(0, true);
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer2((
      content::Source<content::WebContentsDelegate>(browser())));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  new_tab_observer2.Wait();

  // Wait for the navigation in the new tab to finish, if it hasn't, then
  // send it to post_message.html on the original site.
  content::WebContents* new_contents = browser()->GetSelectedWebContents();
  ui_test_utils::WaitForLoadStop(new_contents);
  EXPECT_EQ("/files/title2.html", new_contents->GetURL().path());
  EXPECT_EQ(1, browser()->active_index());
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/post_message.html"));
  EXPECT_EQ(orig_site_instance, new_contents->GetSiteInstance());
  RenderViewHostManager* new_manager =
      static_cast<WebContentsImpl*>(new_contents)->GetRenderManagerForTesting();

  // We now have three windows.  The opener should have a swapped out RVH
  // for the new SiteInstance, but the _blank window should not.
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_TRUE(opener_manager->GetSwappedOutRenderViewHost(foo_site_instance));
  EXPECT_FALSE(new_manager->GetSwappedOutRenderViewHost(foo_site_instance));

  // 2) Fail to post a message from the foo window to the opener if the target
  // origin is wrong.  We won't see an error, but we can check for the right
  // number of received messages below.
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      foo_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(postToOpener('msg',"
      L"'http://google.com'));",
      &success));
  EXPECT_TRUE(success);

  // 3) Post a message from the foo window to the opener.  The opener will
  // reply, causing the foo window to update its own title.
  ui_test_utils::WindowedNotificationObserver title_observer(
        content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
        content::Source<content::WebContents>(foo_contents));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      foo_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(postToOpener('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  title_observer.Wait();

  // We should have received only 1 message in the opener and "foo" tabs,
  // and updated the title.
  int opener_received_messages = 0;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      opener_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(window.receivedMessages);",
      &opener_received_messages));
  int foo_received_messages = 0;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      foo_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, opener_received_messages);
  EXPECT_EQ(ASCIIToUTF16("msg"), foo_contents->GetTitle());

  // 4) Now post a message from the _blank window to the foo window.  The
  // foo window will update its title and will not reply.
  ui_test_utils::WindowedNotificationObserver title_observer2(
        content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
        content::Source<content::WebContents>(foo_contents));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      new_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(postToFoo('msg2'));",
      &success));
  EXPECT_TRUE(success);
  title_observer2.Wait();
  EXPECT_EQ(ASCIIToUTF16("msg2"), foo_contents->GetTitle());

  // This postMessage should have created a swapped out RVH for the new
  // SiteInstance in the target=_blank window.
  EXPECT_TRUE(new_manager->GetSwappedOutRenderViewHost(foo_site_instance));
}

// Test for crbug.com/116192.  Navigations to a window's opener should
// still work after a process swap.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       AllowTargetedNavigationsInOpenerAfterSwap) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original tab and SiteInstance for later comparison.
  content::WebContents* orig_contents = browser()->GetSelectedWebContents();
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=foo link.
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  new_tab_observer.Wait();

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  EXPECT_EQ("/files/navigate_opener.html",
            browser()->GetSelectedWebContents()->GetURL().path());
  EXPECT_EQ(1, browser()->active_index());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the original (opener) tab to a different site.
  browser()->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The opened tab should be able to navigate the opener back to its process.
  browser()->ActivateTabAt(1, true);
  ui_test_utils::WindowedNotificationObserver navigation_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &orig_contents->GetController()));
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(navigateOpener());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Active tab should not have changed.
  EXPECT_EQ(1, browser()->active_index());

  // Should have swapped back into this process.
  browser()->ActivateTabAt(0, true);
  scoped_refptr<SiteInstance> revisit_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);
}

// Test that opening a new window in the same SiteInstance and then navigating
// both windows to a different SiteInstance allows the first process to exit.
// See http://crbug.com/126333.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       ProcessExitWithSwappedOutViews) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=foo link.
  ui_test_utils::WindowedTabAddedNotificationObserver new_tab_observer((
      content::Source<content::WebContentsDelegate>(browser())));
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  new_tab_observer.Wait();

  // Opens in new tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());
  EXPECT_EQ("/files/navigate_opener.html",
            browser()->GetSelectedWebContents()->GetURL().path());
  EXPECT_EQ(1, browser()->active_index());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> opened_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, opened_site_instance);

  // Now navigate the opened tab to a different site.
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The original process should still be alive, since it is still used in the
  // first tab.
  content::RenderProcessHost* orig_process = orig_site_instance->GetProcess();
  EXPECT_TRUE(orig_process->HasConnection());

  // Navigate the first tab to a different site as well.  The original process
  // should exit, since all of its views are now swapped out.
  browser()->ActivateTabAt(0, true);
  ui_test_utils::WindowedNotificationObserver exit_observer(
        content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
        content::Source<content::RenderProcessHost>(orig_process));
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title1.html"));
  exit_observer.Wait();
  scoped_refptr<SiteInstance> new_site_instance2(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(new_site_instance, new_site_instance2);
}

// Test for crbug.com/76666.  A cross-site navigation that fails with a 204
// error should not make us ignore future renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, ClickLinkAfter204Error) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  // The links will point to the HTTPS server.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Load a cross-site page that fails with a 204 error.
  ui_test_utils::NavigateToURL(browser(), https_server.GetURL("nocontent"));

  // We should still be looking at the normal page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/files/click-noreferrer-links.html",
            browser()->GetSelectedWebContents()->GetURL().path());

  // Renderer-initiated navigations should work.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedWebContents());

  // Opens in same tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedWebContents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      browser()->GetSelectedWebContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for http://crbug.com/93427.  Ensure that cross-site navigations
// do not cause back/forward navigations to be considered stale by the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, BackForwardNotStale) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Visit a page on first site.
  std::string replacement_path_a1;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title1.html",
      test_server()->host_port_pair(),
      &replacement_path_a1));
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(replacement_path_a1));

  // Visit three pages on second site.
  std::string replacement_path_b1;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title1.html",
      https_server.host_port_pair(),
      &replacement_path_b1));
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL(replacement_path_b1));
  std::string replacement_path_b2;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title2.html",
      https_server.host_port_pair(),
      &replacement_path_b2));
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL(replacement_path_b2));
  std::string replacement_path_b3;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title3.html",
      https_server.host_port_pair(),
      &replacement_path_b3));
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL(replacement_path_b3));

  // History is now [blank, A1, B1, B2, *B3].
  content::WebContents* contents = browser()->GetSelectedWebContents();
  EXPECT_EQ(5, contents->GetController().GetEntryCount());

  // Open another tab in same process to keep this process alive.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server.GetURL(replacement_path_b1),
      NEW_BACKGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Go back three times to first site.
  {
    ui_test_utils::WindowedNotificationObserver back_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoBack(CURRENT_TAB);
    back_nav_load_observer.Wait();
  }
  {
    ui_test_utils::WindowedNotificationObserver back_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoBack(CURRENT_TAB);
    back_nav_load_observer.Wait();
  }
  {
    ui_test_utils::WindowedNotificationObserver back_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoBack(CURRENT_TAB);
    back_nav_load_observer.Wait();
  }

  // Now go forward twice to B2.  Shouldn't be left spinning.
  {
    ui_test_utils::WindowedNotificationObserver forward_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoForward(CURRENT_TAB);
    forward_nav_load_observer.Wait();
  }
  {
    ui_test_utils::WindowedNotificationObserver forward_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoForward(CURRENT_TAB);
    forward_nav_load_observer.Wait();
  }

  // Go back twice to first site.
  {
    ui_test_utils::WindowedNotificationObserver back_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoBack(CURRENT_TAB);
    back_nav_load_observer.Wait();
  }
  {
    ui_test_utils::WindowedNotificationObserver back_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    browser()->GoBack(CURRENT_TAB);
    back_nav_load_observer.Wait();
  }

  // Now go forward directly to B3.  Shouldn't be left spinning.
  {
    ui_test_utils::WindowedNotificationObserver forward_nav_load_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &contents->GetController()));
    contents->GetController().GoToIndex(4);
    forward_nav_load_observer.Wait();
  }
}

// This class holds onto RenderViewHostObservers for as long as their observed
// RenderViewHosts are alive. This allows us to confirm that all hosts have
// properly been shutdown.
class RenderViewHostObserverArray {
 public:
  ~RenderViewHostObserverArray() {
    // In case some would be left in there with a dead pointer to us.
    for (std::list<RVHObserver*>::iterator iter = observers_.begin();
         iter != observers_.end(); ++iter) {
      (*iter)->ClearParent();
    }
  }
  void AddObserverToRVH(RenderViewHost* rvh) {
    observers_.push_back(new RVHObserver(this, rvh));
  }
  size_t GetNumObservers() const {
    return observers_.size();
  }
 private:
  friend class RVHObserver;
  class RVHObserver : public content::RenderViewHostObserver {
   public:
    RVHObserver(RenderViewHostObserverArray* parent, RenderViewHost* rvh)
        : content::RenderViewHostObserver(rvh),
          parent_(parent) {
    }
    virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE {
      if (parent_)
        parent_->RemoveObserver(this);
      content::RenderViewHostObserver::RenderViewHostDestroyed(rvh);
    };
    void ClearParent() {
      parent_ = NULL;
    }
   private:
    RenderViewHostObserverArray* parent_;
  };

  void RemoveObserver(RVHObserver* observer) {
    observers_.remove(observer);
  }

  std::list<RVHObserver*> observers_;
};

// Test for crbug.com/90867. Make sure we don't leak render view hosts since
// they may cause crashes or memory corruptions when trying to call dead
// delegate_.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, LeakingRenderViewHosts) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               net::TestServer::kLocalhost,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Create a new tab so that we can close the one we navigate and still have
  // a running browser.
  AddBlankTabAndShow(browser());

  // Load a random page and then navigate to view-source: of it.
  // This used to cause two RVH instances for the same SiteInstance, which
  // was a problem.  This is no longer the case.
  GURL navigated_url(test_server()->GetURL("files/title2.html"));
  ui_test_utils::NavigateToURL(browser(), navigated_url);
  SiteInstance* site_instance1 = browser()->GetSelectedWebContents()->
      GetRenderViewHost()->GetSiteInstance();

  // Observe the newly created render_view_host to make sure it will not leak.
  RenderViewHostObserverArray rvh_observers;
  rvh_observers.AddObserverToRVH(browser()->GetSelectedWebContents()->
      GetRenderViewHost());

  GURL view_source_url(chrome::kViewSourceScheme + std::string(":") +
      navigated_url.spec());
  ui_test_utils::NavigateToURL(browser(), view_source_url);
  rvh_observers.AddObserverToRVH(browser()->GetSelectedWebContents()->
      GetRenderViewHost());
  SiteInstance* site_instance2 = browser()->GetSelectedWebContents()->
      GetRenderViewHost()->GetSiteInstance();

  // Ensure that view-source navigations force a new SiteInstance.
  EXPECT_NE(site_instance1, site_instance2);

  // Now navigate to a different instance so that we swap out again.
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title2.html"));
  rvh_observers.AddObserverToRVH(browser()->GetSelectedWebContents()->
      GetRenderViewHost());

  // This used to leak a render view host.
  browser()->CloseTabContents(browser()->GetSelectedWebContents());
  EXPECT_EQ(0U, rvh_observers.GetNumObservers());
}
