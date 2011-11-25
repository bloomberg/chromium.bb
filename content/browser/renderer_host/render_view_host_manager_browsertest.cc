// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

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
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
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
            browser()->GetSelectedTabContents()->GetURL().path());

  // Wait for the cross-site transition in the new tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedTabContents());
  EXPECT_FALSE(browser()->GetSelectedTabContents()->
                   render_manager_for_testing()->pending_render_view_host());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
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
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a target=blank link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
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
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedTabContents());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedTabContents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
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
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedTabContents());

  // Opens in same tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedTabContents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for crbug.com/76666.  A cross-site navigation that fails with a 204
// error should not make us ignore future renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(RenderViewHostManagerTest, ClickLinkAfter204Error) {
  // Start two servers with different sites.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
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
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance != NULL);

  // Load a cross-site page that fails with a 204 error.
  ui_test_utils::NavigateToURL(browser(), https_server.GetURL("nocontent"));

  // We should still be looking at the normal page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/files/click-noreferrer-links.html",
            browser()->GetSelectedTabContents()->GetURL().path());

  // Renderer-initiated navigations should work.
  bool success = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  ui_test_utils::WaitForLoadStop(browser()->GetSelectedTabContents());

  // Opens in same tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_EQ("/files/title2.html",
            browser()->GetSelectedTabContents()->GetURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> noref_site_instance(
      browser()->GetSelectedTabContents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, noref_site_instance);
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
  rvh_observers.AddObserverToRVH(browser()->GetSelectedTabContents()->
      render_view_host());

  GURL view_source_url(chrome::kViewSourceScheme + std::string(":") +
      navigated_url.spec());
  ui_test_utils::NavigateToURL(browser(), view_source_url);
  rvh_observers.AddObserverToRVH(browser()->GetSelectedTabContents()->
      render_view_host());

  // Now navigate to a different instance so that we swap out again.
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("files/title2.html"));
  rvh_observers.AddObserverToRVH(browser()->GetSelectedTabContents()->
      render_view_host());

  // This used to leak a render view host.
  browser()->CloseTabContents(browser()->GetSelectedTabContents());
  EXPECT_EQ(0U, rvh_observers.GetNumObservers());
}
