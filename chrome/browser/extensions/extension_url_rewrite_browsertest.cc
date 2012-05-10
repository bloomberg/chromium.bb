// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"

using content::NavigationEntry;

class ExtensionURLRewriteBrowserTest : public ExtensionBrowserTest {
 protected:
  std::string GetLocationBarText() const {
    return UTF16ToUTF8(
        browser()->window()->GetLocationBar()->GetLocationEntry()->GetText());
  }

  GURL GetLocationBarTextAsURL() const {
    return GURL(GetLocationBarText());
  }

  content::NavigationController* GetNavigationController() const {
    return &browser()->GetSelectedWebContents()->GetController();
  }

  NavigationEntry* GetNavigationEntry() const {
    return GetNavigationController()->GetActiveEntry();
  }

  FilePath GetTestExtensionPath(const char* extension_name) const {
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
        GetNavigationEntry()->GetURL().SchemeIs(chrome::kExtensionScheme));
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
  // Check that the actual URL corresponds to chrome://newtab.
  EXPECT_EQ(url, GetNavigationEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, NewTabPageURLOverride) {
  // Load an extension to override the NTP and check that the location bar text
  // is blank after navigating to chrome://newtab.
  LoadExtension(GetTestExtensionPath("newtab"));
  TestURLNotShown(GURL(chrome::kChromeUINewTabURL));
  // Check that the internal URL uses the chrome-extension:// scheme.
  EXPECT_TRUE(GetNavigationEntry()->GetURL().SchemeIs(chrome::kExtensionScheme));
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURL) {
  // Navigate to chrome://bookmarks and check that the location bar URL is
  // what was entered and the internal URL uses the chrome-extension:// scheme.
  TestExtensionURLOverride(GURL(chrome::kChromeUIBookmarksURL));
}

#if defined(FILE_MANAGER_EXTENSION)
IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, FileManagerURL) {
  // Navigate to chrome://files and check that the location bar URL is
  // what was entered and the internal URL uses the chrome-extension:// scheme.
  TestExtensionURLOverride(GURL(chrome::kChromeUIFileManagerURL));
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURLWithRef) {
  // Navigate to chrome://bookmarks/#1 and check that the location bar URL is
  // what was entered and the internal URL uses the chrome-extension:// scheme.
  GURL url_with_ref(chrome::kChromeUIBookmarksURL + std::string("#1"));
  TestExtensionURLOverride(url_with_ref);
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURLOverride) {
  // Load an extension that overrides chrome://bookmarks.
  LoadExtension(GetTestExtensionPath("bookmarks"));
  // Navigate to chrome://bookmarks and check that the location bar URL is what
  // was entered and the internal URL uses the chrome-extension:// scheme.
  TestExtensionURLOverride(GURL(chrome::kChromeUIBookmarksURL));
}
