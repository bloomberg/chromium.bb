// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

namespace {

const char kTestHtml[] = "files/viewsource/test.html";

class ViewSourceTest : public UITest {
 protected:
  ViewSourceTest()
      : test_server_(net::TestServer::TYPE_HTTP,
                     FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  }

  bool IsMenuCommandEnabled(int command) {
    scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    EXPECT_TRUE(window_proxy.get());
    if (!window_proxy.get())
      return false;

    bool enabled;
    EXPECT_TRUE(window_proxy->IsMenuCommandEnabled(command, &enabled));
    return enabled;
  }

 protected:
  net::TestServer test_server_;
};

// This test renders a page in view-source and then checks to see if a cookie
// set in the html was set successfully (it shouldn't because we rendered the
// page in view source).
// Flaky; see http://crbug.com/72201.
TEST_F(ViewSourceTest, FLAKY_DoesBrowserRenderInViewSource) {
  ASSERT_TRUE(test_server_.Start());

  std::string cookie = "viewsource_cookie";
  std::string cookie_data = "foo";

  // First we navigate to our view-source test page.
  GURL url(chrome::kViewSourceScheme + std::string(":") +
      test_server_.GetURL(kTestHtml).spec());
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(url));

  // Try to retrieve the cookie that the page sets.  It should not be there
  // (because we are in view-source mode).
  std::string cookie_found;
  ASSERT_TRUE(tab->GetCookieByName(url, cookie, &cookie_found));
  EXPECT_NE(cookie_data, cookie_found);
}

// This test renders a page normally and then renders the same page in
// view-source mode. This is done since we had a problem at one point during
// implementation of the view-source: prefix being consumed (removed from the
// URL) if the URL was not changed (apart from adding the view-source prefix)
TEST_F(ViewSourceTest, DoesBrowserConsumeViewSourcePrefix) {
  ASSERT_TRUE(test_server_.Start());

  // First we navigate to google.html.
  GURL url(test_server_.GetURL(kTestHtml));
  NavigateToURL(url);

  // Then we navigate to the same url but with the "view-source:" prefix.
  GURL url_viewsource(chrome::kViewSourceScheme + std::string(":") +
      url.spec());
  NavigateToURL(url_viewsource);

  // The URL should still be prefixed with "view-source:".
  EXPECT_EQ(url_viewsource.spec(), GetActiveTabURL().spec());
}

// Make sure that when looking at the actual page, we can select "View Source"
// from the menu.
TEST_F(ViewSourceTest, ViewSourceInMenuEnabledOnANormalPage) {
  ASSERT_TRUE(test_server_.Start());

  GURL url(test_server_.GetURL(kTestHtml));
  NavigateToURL(url);

  EXPECT_TRUE(IsMenuCommandEnabled(IDC_VIEW_SOURCE));
}

// Make sure that when looking at the page source, we can't select "View Source"
// from the menu.
//
// Occasionally crashes on all platforms, see http://crbug.com/69249
TEST_F(ViewSourceTest, FLAKY_ViewSourceInMenuDisabledWhileViewingSource) {
  ASSERT_TRUE(test_server_.Start());

  GURL url_viewsource(chrome::kViewSourceScheme + std::string(":") +
      test_server_.GetURL(kTestHtml).spec());
  NavigateToURL(url_viewsource);

  EXPECT_FALSE(IsMenuCommandEnabled(IDC_VIEW_SOURCE));
}

}  // namespace
