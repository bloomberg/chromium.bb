// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/referrer.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::Referrer;

namespace {
struct AboutURLTestCase {
  GURL test_url;
  GURL expected_url;
};
}

class BrowserAboutHandlerTest : public testing::Test {
 protected:
  void TestWillHandleBrowserAboutURL(
      const std::vector<AboutURLTestCase>& test_cases) {
    TestingProfile profile;

    for (const auto& test_case : test_cases) {
      GURL url(test_case.test_url);
      WillHandleBrowserAboutURL(&url, &profile);
      EXPECT_EQ(test_case.expected_url, url);
    }
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(BrowserAboutHandlerTest, WillHandleBrowserAboutURL) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  std::vector<AboutURLTestCase> test_cases(
      {{GURL("http://google.com"), GURL("http://google.com")},
       {GURL(url::kAboutBlankURL), GURL(url::kAboutBlankURL)},
       {GURL(chrome_prefix + chrome::kChromeUIDefaultHost),
        GURL(chrome_prefix + chrome::kChromeUIVersionHost)},
       {GURL(chrome_prefix + chrome::kChromeUIAboutHost),
        GURL(chrome_prefix + chrome::kChromeUIChromeURLsHost)},
       {GURL(chrome_prefix + chrome::kChromeUICacheHost),
        GURL(chrome_prefix + content::kChromeUINetworkViewCacheHost)},
       {GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost),
        GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost)},
       {GURL(chrome_prefix + chrome::kChromeUISyncHost),
        GURL(chrome_prefix + chrome::kChromeUISyncInternalsHost)},
       {GURL(chrome_prefix + chrome::kChromeUIInvalidationsHost),
        GURL(chrome_prefix + chrome::kChromeUIInvalidationsHost)},
       {
           GURL(chrome_prefix + "host/path?query#ref"),
           GURL(chrome_prefix + "host/path?query#ref"),
       }});
  TestWillHandleBrowserAboutURL(test_cases);
}

TEST_F(BrowserAboutHandlerTest, WillHandleBrowserAboutURLForMDSettings) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  std::vector<AboutURLTestCase> test_cases(
      {{GURL(chrome_prefix + chrome::kChromeUISettingsHost),
        GURL(chrome_prefix + chrome::kChromeUISettingsHost)}});
  TestWillHandleBrowserAboutURL(test_cases);
}

TEST_F(BrowserAboutHandlerTest, WillHandleBrowserAboutURLForHistory) {
  TestWillHandleBrowserAboutURL(std::vector<AboutURLTestCase>({
      {GURL("about:history"), GURL("chrome://history/")},
      {GURL("about:history-frame"), GURL("chrome://history/")},
      {GURL("chrome://history"), GURL("chrome://history/")},
      {GURL("chrome://history-frame"), GURL("chrome://history/")},
      {GURL("chrome://history/"), GURL("chrome://history/")},
      {GURL("chrome://history-frame/"), GURL("chrome://history/")},
      {GURL("chrome://history/?q=foo"), GURL("chrome://history/?q=foo")},
      {GURL("chrome://history-frame/?q=foo"), GURL("chrome://history/?q=foo")},
  }));
}

// Ensure that minor BrowserAboutHandler fixup to a URL does not cause us to
// keep a separate virtual URL, which would not be updated on redirects.
// See https://crbug.com/449829.
TEST_F(BrowserAboutHandlerTest, NoVirtualURLForFixup) {
  GURL url("view-source:http://.foo");

  // Fixup will remove the dot and add a slash.
  GURL fixed_url("view-source:http://foo/");

  // Rewriters will remove the view-source prefix and expect it to stay in the
  // virtual URL.
  GURL rewritten_url("http://foo/");

  TestingProfile profile;
  std::unique_ptr<NavigationEntry> entry(
      NavigationController::CreateNavigationEntry(
          url, Referrer(), ui::PAGE_TRANSITION_RELOAD, false, std::string(),
          &profile));
  EXPECT_EQ(fixed_url, entry->GetVirtualURL());
  EXPECT_EQ(rewritten_url, entry->GetURL());
}
