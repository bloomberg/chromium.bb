// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

class ToolbarModelTest : public BrowserWithTestWindowTest {
 public:
  ToolbarModelTest() {}

 protected:
  void NavigateAndCheckText(const std::string& url,
                            const std::string& expected_text,
                            bool should_display) {
    WebContents* contents = chrome::GetWebContentsAt(browser(), 0);
    browser()->OpenURL(OpenURLParams(
        GURL(url), Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
        false));

    // Check while loading.
    EXPECT_EQ(should_display, browser()->toolbar_model()->ShouldDisplayURL());
    EXPECT_EQ(ASCIIToUTF16(expected_text),
              browser()->toolbar_model()->GetText());

    // Check after commit.
    CommitPendingLoad(&contents->GetController());
    EXPECT_EQ(should_display, browser()->toolbar_model()->ShouldDisplayURL());
    EXPECT_EQ(ASCIIToUTF16(expected_text),
              browser()->toolbar_model()->GetText());
  }
};

// Test that URLs are correctly shown or hidden both during navigation and
// after commit.
TEST_F(ToolbarModelTest, ShouldDisplayURL) {
  AddTab(browser(), GURL(chrome::kAboutBlankURL));

  NavigateAndCheckText("view-source:http://www.google.com",
                       "view-source:www.google.com", true);
  NavigateAndCheckText("view-source:chrome://newtab/",
                       "view-source:chrome://newtab", true);
  NavigateAndCheckText("chrome-extension://monkey/balls.html", "", false);
  NavigateAndCheckText("chrome://newtab/", "", false);
  NavigateAndCheckText(chrome::kAboutBlankURL, chrome::kAboutBlankURL, true);
}
