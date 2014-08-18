// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

class InterstitialUITest : public InProcessBrowserTest {
 public:
   InterstitialUITest() {}
   virtual ~InterstitialUITest() {}

 protected:
  void TestInterstitial(GURL url, const std::string& page_title) {
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(
      base::ASCIIToUTF16(page_title),
      browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

    // Should also be able to open and close devtools.
    DevToolsWindow* window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
    EXPECT_TRUE(window);
    DevToolsWindowTesting::CloseDevToolsWindowSync(window);
  }
};

IN_PROC_BROWSER_TEST_F(InterstitialUITest, OpenInterstitial) {
  TestInterstitial(
      GURL("chrome://interstitials"),
      "Interstitials");
  // Invalid path should open the main page:
  TestInterstitial(
      GURL("chrome://interstitials/--invalid--"),
      "Interstitials");
  TestInterstitial(
      GURL("chrome://interstitials/ssl"),
      "Privacy error");
  TestInterstitial(
      GURL("chrome://interstitials/safebrowsing?type=malware"),
      "Security error");
  TestInterstitial(
      GURL("chrome://interstitials/safebrowsing?type=phishing"),
      "Security error");
  TestInterstitial(
      GURL("chrome://interstitials/safebrowsing?type=clientside_malware"),
      "Security error");
  TestInterstitial(
      GURL("chrome://interstitials/safebrowsing?type=clientside_phishing"),
      "Security error");
}
