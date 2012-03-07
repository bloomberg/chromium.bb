// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
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
  TabContents* tab_contents = static_cast<TabContents*>(
      browser()->GetSelectedWebContents());
  EXPECT_FALSE(tab_contents->GetRenderManagerForTesting()->
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
  TabContents* tab_contents = static_cast<TabContents*>(
      browser()->GetSelectedWebContents());
  EXPECT_FALSE(tab_contents->GetRenderManagerForTesting()->
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
  // This is one way to cause two rvh instances for the same instance id.
  GURL navigated_url(test_server()->GetURL("files/title2.html"));
  ui_test_utils::NavigateToURL(browser(), navigated_url);

  // Observe the newly created render_view_host to make sure it will not leak.
  RenderViewHostObserverArray rvh_observers;
  rvh_observers.AddObserverToRVH(browser()->GetSelectedWebContents()->
      GetRenderViewHost());

  GURL view_source_url(chrome::kViewSourceScheme + std::string(":") +
      navigated_url.spec());
  ui_test_utils::NavigateToURL(browser(), view_source_url);
  rvh_observers.AddObserverToRVH(browser()->GetSelectedWebContents()->
      GetRenderViewHost());

  // Now navigate to a different instance so that we swap out again.
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title2.html"));
  rvh_observers.AddObserverToRVH(browser()->GetSelectedWebContents()->
      GetRenderViewHost());

  // This used to leak a render view host.
  browser()->CloseTabContents(browser()->GetSelectedWebContents());
  EXPECT_EQ(0U, rvh_observers.GetNumObservers());
}
