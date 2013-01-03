// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/common/content_constants_internal.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

namespace content {
namespace {

bool CompareTrees(base::DictionaryValue* first, base::DictionaryValue* second) {
  string16 name1;
  string16 name2;
  if (!first->GetString(kFrameTreeNodeNameKey, &name1) ||
      !second->GetString(kFrameTreeNodeNameKey, &name2))
    return false;
  if (name1 != name2)
    return false;

  int id1 = 0;
  int id2 = 0;
  if (!first->GetInteger(kFrameTreeNodeIdKey, &id1) ||
      !second->GetInteger(kFrameTreeNodeIdKey, &id2)) {
    return false;
  }
  if (id1 != id2)
    return false;

  ListValue* subtree1 = NULL;
  ListValue* subtree2 = NULL;
  bool result1 = first->GetList(kFrameTreeNodeSubtreeKey, &subtree1);
  bool result2 = second->GetList(kFrameTreeNodeSubtreeKey, &subtree2);
  if (!result1 && !result2)
    return true;
  if (!result1 || !result2)
    return false;

  if (subtree1->GetSize() != subtree2->GetSize())
    return false;

  base::DictionaryValue* child1 = NULL;
  base::DictionaryValue* child2 = NULL;
  for (size_t i = 0; i < subtree1->GetSize(); ++i) {
    if (!subtree1->GetDictionary(i, &child1) ||
        !subtree2->GetDictionary(i, &child2)) {
      return false;
    }
    if (!CompareTrees(child1, child2))
      return false;
  }

  return true;
}

base::DictionaryValue* GetTree(RenderViewHostImpl* rvh) {
  std::string frame_tree = rvh->frame_tree();
  EXPECT_FALSE(frame_tree.empty());
  base::Value* v = base::JSONReader::Read(frame_tree);
  base::DictionaryValue* tree = NULL;
  EXPECT_TRUE(v->IsType(base::Value::TYPE_DICTIONARY));
  EXPECT_TRUE(v->GetAsDictionary(&tree));
  return tree;
}

} // namespace

class RenderViewHostManagerTest : public ContentBrowserTest {
 public:
  RenderViewHostManagerTest() {}

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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Open a same-site link in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // We should no longer have script access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(testScriptAccessToWindow());",
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/files/title2.html", new_shell->web_contents()->GetURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a same-site rel=noreferrer + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteNoRefTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/files/title2.html", new_shell->web_contents()->GetURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/files/title2.html", shell()->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Clicking the original link in the first tab should cause us to swap back.
  WindowedNotificationObserver navigation_observer(
      NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(
          &new_shell->web_contents()->GetController()));
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back and shown the new window again.
  scoped_refptr<SiteInstance> revisit_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);

  // If it navigates away to another process, the original window should
  // still be able to close it (using a cross-process close message).
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  EXPECT_EQ(new_site_instance,
            new_shell->web_contents()->GetSiteInstance());
  WindowedNotificationObserver close_observer(
        NOTIFICATION_WEB_CONTENTS_DESTROYED,
        Source<WebContents>(new_shell->web_contents()));
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(testCloseWindow());",
      &success));
  EXPECT_TRUE(success);
  close_observer.Wait();
}

// Test that setting the opener to null in a window affects cross-process
// navigations, including those to existing entries.  http://crbug.com/156669.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, DisownOpener) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=_blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title2.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Now disown the opener.
  EXPECT_TRUE(ExecuteJavaScript(
      new_shell->web_contents()->GetRenderViewHost(),
      "",
      "window.opener = null;"));

  // Go back and ensure the opener is still null.
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(
            &new_shell->web_contents()->GetController()));
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      new_shell->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Now navigate forward again (creating a new process) and check opener.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      new_shell->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/99202.  PostMessage calls should still work after
// navigating the source and target windows to different sites.
// Specifically:
// 1) Create 3 windows (opener, "foo", and _blank) and send "foo" cross-process.
// 2) Fail to post a message from "foo" to opener with the wrong target origin.
// 3) Post a message from "foo" to opener, which replies back to "foo".
// 4) Post a message from _blank to "foo".
// 5) Post a message from "foo" to a subframe of opener, which replies back.
// 6) Post a message from _blank to a subframe of "foo".
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SupportCrossProcessPostMessage) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);
  RenderViewHostManager* opener_manager =
      static_cast<WebContentsImpl*>(opener_contents)->
          GetRenderManagerForTesting();

  // 1) Open two more windows, one named.  These initially have openers but no
  // reference to each other.  We will later post a message between them.

  // First, a named target=foo window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      opener_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/files/navigate_opener.html", foo_contents->GetURL().path());
  NavigateToURL(new_shell, https_server.GetURL("files/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // Second, a target=_blank window.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on the original site.
  Shell* new_shell2 = new_shell_observer2.GetShell();
  WebContents* new_contents = new_shell2->web_contents();
  WaitForLoadStop(new_contents);
  EXPECT_EQ("/files/title2.html", new_contents->GetURL().path());
  NavigateToURL(new_shell2, test_server()->GetURL("files/post_message.html"));
  EXPECT_EQ(orig_site_instance, new_contents->GetSiteInstance());
  RenderViewHostManager* new_manager =
      static_cast<WebContentsImpl*>(new_contents)->GetRenderManagerForTesting();

  // We now have three windows.  The opener should have a swapped out RVH
  // for the new SiteInstance, but the _blank window should not.
  EXPECT_EQ(3u, Shell::windows().size());
  EXPECT_TRUE(opener_manager->GetSwappedOutRenderViewHost(foo_site_instance));
  EXPECT_FALSE(new_manager->GetSwappedOutRenderViewHost(foo_site_instance));

  // 2) Fail to post a message from the foo window to the opener if the target
  // origin is wrong.  We won't see an error, but we can check for the right
  // number of received messages below.
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      foo_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(postToOpener('msg',"
          "'http://google.com'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(opener_manager->GetSwappedOutRenderViewHost(orig_site_instance));

  // 3) Post a message from the foo window to the opener.  The opener will
  // reply, causing the foo window to update its own title.
  WindowedNotificationObserver title_observer(
      NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
      Source<WebContents>(foo_contents));
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      foo_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(postToOpener('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(opener_manager->GetSwappedOutRenderViewHost(orig_site_instance));
  title_observer.Wait();

  // We should have received only 1 message in the opener and "foo" tabs,
  // and updated the title.
  int opener_received_messages = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractInt(
      opener_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(window.receivedMessages);",
      &opener_received_messages));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractInt(
      foo_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, opener_received_messages);
  EXPECT_EQ(ASCIIToUTF16("msg"), foo_contents->GetTitle());

  // 4) Now post a message from the _blank window to the foo window.  The
  // foo window will update its title and will not reply.
  WindowedNotificationObserver title_observer2(
      NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
      Source<WebContents>(foo_contents));
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      new_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(postToFoo('msg2'));",
      &success));
  EXPECT_TRUE(success);
  title_observer2.Wait();
  EXPECT_EQ(ASCIIToUTF16("msg2"), foo_contents->GetTitle());

  // This postMessage should have created a swapped out RVH for the new
  // SiteInstance in the target=_blank window.
  EXPECT_TRUE(new_manager->GetSwappedOutRenderViewHost(foo_site_instance));

  // TODO(nasko): Test subframe targeting of postMessage once
  // http://crbug.com/153701 is fixed.
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original tab and SiteInstance for later comparison.
  WebContents* orig_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      orig_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      orig_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the original (opener) tab to a different site.
  NavigateToURL(shell(), https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The opened tab should be able to navigate the opener back to its process.
  WindowedNotificationObserver navigation_observer(
      NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(
          &orig_contents->GetController()));
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      new_shell->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(navigateOpener());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back into this process.
  scoped_refptr<SiteInstance> revisit_site_instance(
      shell()->web_contents()->GetSiteInstance());
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> opened_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, opened_site_instance);

  // Now navigate the opened window to a different site.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The original process should still be alive, since it is still used in the
  // first window.
  RenderProcessHost* orig_process = orig_site_instance->GetProcess();
  EXPECT_TRUE(orig_process->HasConnection());

  // Navigate the first window to a different site as well.  The original
  // process should exit, since all of its views are now swapped out.
  WindowedNotificationObserver exit_observer(
      NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      Source<RenderProcessHost>(orig_process));
  NavigateToURL(shell(), https_server.GetURL("files/title1.html"));
  exit_observer.Wait();
  scoped_refptr<SiteInstance> new_site_instance2(
      shell()->web_contents()->GetSiteInstance());
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
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  // The links will point to the HTTPS server.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Load a cross-site page that fails with a 204 error.
  NavigateToURL(shell(), https_server.GetURL("nocontent"));

  // We should still be looking at the normal page.  The typed URL will
  // still be visible until the user clears it manually, but the last
  // committed URL will be the previous page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/nocontent", shell()->web_contents()->GetURL().path());
  EXPECT_EQ("/files/click-noreferrer-links.html",
            shell()->web_contents()->GetController().
                GetLastCommittedEntry()->GetVirtualURL().path());

  // Renderer-initiated navigations should work.
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same tab.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/files/title2.html", shell()->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for http://crbug.com/93427.  Ensure that cross-site navigations
// do not cause back/forward navigations to be considered stale by the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, BackForwardNotStale) {
  NavigateToURL(shell(), GURL(chrome::kAboutBlankURL));

  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Visit a page on first site.
  std::string replacement_path_a1;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title1.html",
      test_server()->host_port_pair(),
      &replacement_path_a1));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path_a1));

  // Visit three pages on second site.
  std::string replacement_path_b1;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title1.html",
      https_server.host_port_pair(),
      &replacement_path_b1));
  NavigateToURL(shell(), https_server.GetURL(replacement_path_b1));
  std::string replacement_path_b2;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title2.html",
      https_server.host_port_pair(),
      &replacement_path_b2));
  NavigateToURL(shell(), https_server.GetURL(replacement_path_b2));
  std::string replacement_path_b3;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/title3.html",
      https_server.host_port_pair(),
      &replacement_path_b3));
  NavigateToURL(shell(), https_server.GetURL(replacement_path_b3));

  // History is now [blank, A1, B1, B2, *B3].
  WebContents* contents = shell()->web_contents();
  EXPECT_EQ(5, contents->GetController().GetEntryCount());

  // Open another window in same process to keep this process alive.
  Shell* new_shell = CreateBrowser();
  NavigateToURL(new_shell, https_server.GetURL(replacement_path_b1));

  // Go back three times to first site.
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(
            &contents->GetController()));
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(
            &contents->GetController()));
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->GetController()));
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward twice to B2.  Shouldn't be left spinning.
  {
    WindowedNotificationObserver forward_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->GetController()));
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }
  {
    WindowedNotificationObserver forward_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->GetController()));
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }

  // Go back twice to first site.
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->GetController()));
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->GetController()));
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward directly to B3.  Shouldn't be left spinning.
  {
    WindowedNotificationObserver forward_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(&contents->GetController()));
    shell()->web_contents()->GetController().GoToIndex(4);
    forward_nav_load_observer.Wait();
  }
}

// Test for http://crbug.com/130016.
// Swapping out a render view should update its visiblity state.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       SwappedOutViewHasCorrectVisibilityState) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Load a page with links that open in a new window.
  std::string replacement_path;
  ASSERT_TRUE(GetFilePathWithHostAndPortReplacement(
      "files/click-noreferrer-links.html",
      https_server.host_port_pair(),
      &replacement_path));
  NavigateToURL(shell(), test_server()->GetURL(replacement_path));

  // Open a same-site link in a new widnow.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetURL().path());

  RenderViewHost* rvh = new_shell->web_contents()->GetRenderViewHost();
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      rvh,
      "",
      "window.domAutomationController.send("
          "document.webkitVisibilityState == 'visible');",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site. This should swap out the
  // tab's existing RenderView, causing it become hidden.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));

  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      rvh,
      "",
      "window.domAutomationController.send("
          "document.webkitVisibilityState == 'hidden');",
      &success));
  EXPECT_TRUE(success);

  // Going back should make the previously swapped-out view to become visible
  // again.
  {
    WindowedNotificationObserver back_nav_load_observer(
        NOTIFICATION_NAV_ENTRY_COMMITTED,
        Source<NavigationController>(
            &new_shell->web_contents()->GetController()));
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }


  EXPECT_EQ("/files/navigate_opener.html",
            new_shell->web_contents()->GetURL().path());

  EXPECT_EQ(rvh, new_shell->web_contents()->GetRenderViewHost());

  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      rvh,
      "",
      "window.domAutomationController.send("
          "document.webkitVisibilityState == 'visible');",
      &success));
  EXPECT_TRUE(success);
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
  class RVHObserver : public RenderViewHostObserver {
   public:
    RVHObserver(RenderViewHostObserverArray* parent, RenderViewHost* rvh)
        : RenderViewHostObserver(rvh),
          parent_(parent) {
    }
    virtual void RenderViewHostDestroyed(RenderViewHost* rvh) OVERRIDE {
      if (parent_)
        parent_->RemoveObserver(this);
      RenderViewHostObserver::RenderViewHostDestroyed(rvh);
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
// delegate_. This test also verifies crbug.com/117420 and crbug.com/143255 to
// ensure that a separate SiteInstance is created when navigating to view-source
// URLs, regardless of current URL.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, LeakingRenderViewHosts) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Observe the created render_view_host's to make sure they will not leak.
  RenderViewHostObserverArray rvh_observers;

  GURL navigated_url(test_server()->GetURL("files/title2.html"));
  GURL view_source_url(chrome::kViewSourceScheme + std::string(":") +
      navigated_url.spec());

  // Let's ensure that when we start with a blank window, navigating away to a
  // view-source URL, we create a new SiteInstance.
  RenderViewHost* blank_rvh = shell()->web_contents()->
      GetRenderViewHost();
  SiteInstance* blank_site_instance = blank_rvh->GetSiteInstance();
  EXPECT_EQ(shell()->web_contents()->GetURL(), GURL::EmptyGURL());
  EXPECT_EQ(blank_site_instance->GetSiteURL(), GURL::EmptyGURL());
  rvh_observers.AddObserverToRVH(blank_rvh);

  // Now navigate to the view-source URL and ensure we got a different
  // SiteInstance and RenderViewHost.
  NavigateToURL(shell(), view_source_url);
  EXPECT_NE(blank_rvh, shell()->web_contents()->GetRenderViewHost());
  EXPECT_NE(blank_site_instance, shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance());
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());

  // Load a random page and then navigate to view-source: of it.
  // This used to cause two RVH instances for the same SiteInstance, which
  // was a problem.  This is no longer the case.
  NavigateToURL(shell(), navigated_url);
  SiteInstance* site_instance1 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());

  NavigateToURL(shell(), view_source_url);
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());
  SiteInstance* site_instance2 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();

  // Ensure that view-source navigations force a new SiteInstance.
  EXPECT_NE(site_instance1, site_instance2);

  // Now navigate to a different instance so that we swap out again.
  NavigateToURL(shell(), https_server.GetURL("files/title2.html"));
  rvh_observers.AddObserverToRVH(shell()->web_contents()->GetRenderViewHost());

  // This used to leak a render view host.
  shell()->Close();

  RunAllPendingInMessageLoop();  // Needed on ChromeOS.

  EXPECT_EQ(0U, rvh_observers.GetNumObservers());
}

// Test for correct propagation of the frame hierarchy across processes in the
// same BrowsingInstance. The test starts by navigating to a page that has
// multiple nested frames. It then opens two windows and navigates each one
// to a separate site, so at the end we have 3 SiteInstances. The opened
// windows have swapped out RenderViews corresponding to the opener, so those
// swapped out views must have a matching frame hierarchy. The test checks
// that frame hierarchies are kept in sync through navigations, reloading, and
// JavaScript manipulation of the frame tree.
//
// Disable the test until http://crbug.com/153701 is fixed.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, DISABLED_FrameTreeUpdates) {
  // Start two servers to allow using different sites.
  EXPECT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  EXPECT_TRUE(https_server.Start());

  GURL frame_tree_url(test_server()->GetURL("files/frame_tree/top.html"));

  // Replace the 127.0.0.1 with localhost, which will give us a different
  // site instance.
  GURL::Replacements replacements;
  std::string new_host("localhost");
  replacements.SetHostStr(new_host);
  GURL remote_frame = test_server()->GetURL(
      "files/frame_tree/1-1.html").ReplaceComponents(replacements);

  bool success = false;
  base::DictionaryValue* frames = NULL;
  base::ListValue* subtree = NULL;

  // First navigate to a page with no frames and ensure the frame tree has no
  // subtrees.
  NavigateToURL(shell(), test_server()->GetURL("files/simple_page.html"));
  WebContents* opener_contents = shell()->web_contents();
  RenderViewHostManager* opener_rvhm = static_cast<WebContentsImpl*>(
      opener_contents)->GetRenderManagerForTesting();
  frames = GetTree(opener_rvhm->current_host());
  EXPECT_FALSE(frames->GetList(kFrameTreeNodeSubtreeKey, &subtree));

  NavigateToURL(shell(), frame_tree_url);
  frames = GetTree(opener_rvhm->current_host());
  EXPECT_TRUE(frames->GetList(kFrameTreeNodeSubtreeKey, &subtree));
  EXPECT_TRUE(subtree->GetSize() == 3);

  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  ShellAddedObserver shell_observer1;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      opener_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(openWindow('1-3.html'));",
      &success));
  EXPECT_TRUE(success);

  Shell* shell1 = shell_observer1.GetShell();
  WebContents* contents1 = shell1->web_contents();
  WaitForLoadStop(contents1);
  RenderViewHostManager* rvhm1 = static_cast<WebContentsImpl*>(
      contents1)->GetRenderManagerForTesting();
  EXPECT_EQ("/files/frame_tree/1-3.html", contents1->GetURL().path());

  // Now navigate the new window to a different SiteInstance.
  NavigateToURL(shell1, https_server.GetURL("files/title1.html"));
  EXPECT_EQ("/files/title1.html", contents1->GetURL().path());
  scoped_refptr<SiteInstance> site_instance1(
      contents1->GetSiteInstance());
  EXPECT_NE(orig_site_instance, site_instance1);

  ShellAddedObserver shell_observer2;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      opener_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(openWindow('../title2.html'));",
      &success));
  EXPECT_TRUE(success);

  Shell* shell2 = shell_observer2.GetShell();
  WebContents* contents2 = shell2->web_contents();
  WaitForLoadStop(contents2);
  EXPECT_EQ("/files/title2.html", contents2->GetURL().path());

  // Navigate the second new window to a different SiteInstance as well.
  NavigateToURL(shell2, remote_frame);
  EXPECT_EQ("/files/frame_tree/1-1.html", contents2->GetURL().path());
  scoped_refptr<SiteInstance> site_instance2(
      contents2->GetSiteInstance());
  EXPECT_NE(orig_site_instance, site_instance2);
  EXPECT_NE(site_instance1, site_instance2);

  RenderViewHostManager* rvhm2 = static_cast<WebContentsImpl*>(
      contents2)->GetRenderManagerForTesting();

  EXPECT_TRUE(CompareTrees(
      GetTree(opener_rvhm->current_host()),
      GetTree(opener_rvhm->GetSwappedOutRenderViewHost(site_instance1))));
  EXPECT_TRUE(CompareTrees(
      GetTree(opener_rvhm->current_host()),
      GetTree(opener_rvhm->GetSwappedOutRenderViewHost(site_instance2))));

  EXPECT_TRUE(CompareTrees(
      GetTree(rvhm1->current_host()),
      GetTree(rvhm1->GetSwappedOutRenderViewHost(orig_site_instance))));
  EXPECT_TRUE(CompareTrees(
      GetTree(rvhm2->current_host()),
      GetTree(rvhm2->GetSwappedOutRenderViewHost(orig_site_instance))));

  // Verify that the frame trees from different windows aren't equal.
  EXPECT_FALSE(CompareTrees(
      GetTree(opener_rvhm->current_host()), GetTree(rvhm1->current_host())));
  EXPECT_FALSE(CompareTrees(
      GetTree(opener_rvhm->current_host()), GetTree(rvhm2->current_host())));

  // Reload the original page, which will cause subframe ids to change. This
  // will ensure that the ids are properly replicated across reload.
  NavigateToURL(shell(), frame_tree_url);

  EXPECT_TRUE(CompareTrees(
      GetTree(opener_rvhm->current_host()),
      GetTree(opener_rvhm->GetSwappedOutRenderViewHost(site_instance1))));
  EXPECT_TRUE(CompareTrees(
      GetTree(opener_rvhm->current_host()),
      GetTree(opener_rvhm->GetSwappedOutRenderViewHost(site_instance2))));

  EXPECT_FALSE(CompareTrees(
      GetTree(opener_rvhm->current_host()), GetTree(rvhm1->current_host())));
  EXPECT_FALSE(CompareTrees(
      GetTree(opener_rvhm->current_host()), GetTree(rvhm2->current_host())));

  // Now let's ensure that using JS to add/remove frames results in proper
  // updates.
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      opener_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(removeFrame());",
      &success));
  EXPECT_TRUE(success);
  frames = GetTree(opener_rvhm->current_host());
  EXPECT_TRUE(frames->GetList(kFrameTreeNodeSubtreeKey, &subtree));
  EXPECT_EQ(subtree->GetSize(), 2U);

  // Create a load observer for the iframe that will be created by the
  // JavaScript code we will execute.
  WindowedNotificationObserver load_observer(
      NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(
              &opener_contents->GetController()));
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      opener_contents->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(addFrame());",
      &success));
  EXPECT_TRUE(success);
  load_observer.Wait();

  frames = GetTree(opener_rvhm->current_host());
  EXPECT_TRUE(frames->GetList(kFrameTreeNodeSubtreeKey, &subtree));
  EXPECT_EQ(subtree->GetSize(), 3U);

  EXPECT_TRUE(CompareTrees(
      GetTree(opener_rvhm->current_host()),
      GetTree(opener_rvhm->GetSwappedOutRenderViewHost(site_instance1))));
  EXPECT_TRUE(CompareTrees(
      GetTree(opener_rvhm->current_host()),
      GetTree(opener_rvhm->GetSwappedOutRenderViewHost(site_instance2))));
}

// Test for crbug.com/143155.  Frame tree updates during unload should not
// interrupt the intended navigation and show swappedout:// instead.
// Specifically:
// 1) Open 2 tabs in an HTTP SiteInstance, with a subframe in the opener.
// 2) Send the second tab to a different HTTPS SiteInstance.
//    This creates a swapped out opener for the first tab in the HTTPS process.
// 3) Navigate the first tab to the HTTPS SiteInstance, and have the first
//    tab's unload handler remove its frame.
// This used to cause an update to the frame tree of the swapped out RV,
// just as it was navigating to a real page.  That pre-empted the real
// navigation and visibly sent the tab to swappedout://.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest,
                       DontPreemptNavigationWithFrameTreeUpdate) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // 1. Load a page that deletes its iframe during unload.
  NavigateToURL(shell(),
                test_server()->GetURL("files/remove_frame_on_unload.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Open a same-site page in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteJavaScriptAndExtractBool(
      shell()->web_contents()->GetRenderViewHost(),
      "",
      "window.domAutomationController.send(openWindow());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/files/title1.html",
            new_shell->web_contents()->GetURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // 2. Send the second tab to a different process.
  NavigateToURL(new_shell, https_server.GetURL("files/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // 3. Send the first tab to the second tab's process.
  NavigateToURL(shell(), https_server.GetURL("files/title1.html"));

  // Make sure it ends up at the right page.
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(https_server.GetURL("files/title1.html"),
            shell()->web_contents()->GetURL());
  EXPECT_EQ(new_site_instance, shell()->web_contents()->GetSiteInstance());
}

}  // namespace content
