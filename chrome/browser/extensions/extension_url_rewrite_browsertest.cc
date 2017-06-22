// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/md_bookmarks/md_bookmarks_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

using content::NavigationEntry;

class ExtensionURLRewriteBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ExtensionBrowserTest::SetUp();
  }

 protected:
  std::string GetLocationBarText() const {
    return base::UTF16ToUTF8(
        browser()->window()->GetLocationBar()->GetOmniboxView()->GetText());
  }

  GURL GetLocationBarTextAsURL() const {
    return GURL(GetLocationBarText());
  }

  content::NavigationController* GetNavigationController() const {
    return &browser()->tab_strip_model()->GetActiveWebContents()->
        GetController();
  }

  NavigationEntry* GetNavigationEntry() const {
    return GetNavigationController()->GetVisibleEntry();
  }

  base::FilePath GetTestExtensionPath(const char* extension_name) const {
    return test_data_dir_.AppendASCII("browsertest/url_rewrite/").
        AppendASCII(extension_name);
  }

  // Navigates to |url| and tests that the location bar and the |virtual_url|
  // correspond to |url|, while the real URL of the navigation entry uses the
  // chrome-extension:// scheme.
  void TestExtensionURLOverride(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(url, GetLocationBarTextAsURL());
    EXPECT_EQ(url, GetNavigationEntry()->GetVirtualURL());
    EXPECT_TRUE(
        GetNavigationEntry()->GetURL().SchemeIs(extensions::kExtensionScheme));
  }

  // Navigates to |url| and tests that the location bar is empty while the
  // |virtual_url| is the same as |url|.
  void TestURLNotShown(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ("", GetLocationBarText());
    EXPECT_EQ(url, GetNavigationEntry()->GetVirtualURL());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, NewTabPageURL) {
  // Navigate to chrome://newtab and check that the location bar text is blank.
  GURL url(chrome::kChromeUINewTabURL);
  TestURLNotShown(url);
  // Check that the actual URL corresponds to the new tab URL.
  EXPECT_TRUE(search::IsNTPURL(GetNavigationEntry()->GetURL(), profile()));
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, NewTabPageURLOverride) {
  // Load an extension to override the NTP and check that the location bar text
  // is blank after navigating to chrome://newtab.
  ASSERT_TRUE(LoadExtension(GetTestExtensionPath("newtab")));
  TestURLNotShown(GURL(chrome::kChromeUINewTabURL));
  // Check that the internal URL uses the chrome-extension:// scheme.
  EXPECT_TRUE(GetNavigationEntry()->GetURL().SchemeIs(
      extensions::kExtensionScheme));
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURL) {
  if (MdBookmarksUI::IsEnabled())
    return;

  // Navigate to chrome://bookmarks and check that the location bar URL is
  // what was entered and the internal URL uses the chrome-extension:// scheme.
  const GURL bookmarks_url(chrome::kChromeUIBookmarksURL);
  ui_test_utils::NavigateToURL(browser(), bookmarks_url);
  // The default chrome://bookmarks implementation will append /#1 to the URL
  // once loaded. Use |GetWithEmptyPath()| to avoid flakyness.
  EXPECT_EQ(bookmarks_url, GetLocationBarTextAsURL().GetWithEmptyPath());
  NavigationEntry* navigation = GetNavigationEntry();
  EXPECT_EQ(bookmarks_url, navigation->GetVirtualURL().GetWithEmptyPath());
  EXPECT_TRUE(navigation->GetURL().SchemeIs(extensions::kExtensionScheme));
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURLWithRef) {
  if (MdBookmarksUI::IsEnabled())
    return;

  // Navigate to chrome://bookmarks/#1 and check that the location bar URL is
  // what was entered and the internal URL uses the chrome-extension:// scheme.
  GURL url_with_ref(chrome::kChromeUIBookmarksURL + std::string("#1"));
  TestExtensionURLOverride(url_with_ref);
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURLOverride) {
  // Load an extension that overrides chrome://bookmarks.
  ASSERT_TRUE(LoadExtension(GetTestExtensionPath("bookmarks")));
  // Navigate to chrome://bookmarks and check that the location bar URL is what
  // was entered and the internal URL uses the chrome-extension:// scheme.
  TestExtensionURLOverride(GURL(chrome::kChromeUIBookmarksURL));
}
