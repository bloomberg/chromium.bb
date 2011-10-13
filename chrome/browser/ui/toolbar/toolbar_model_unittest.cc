// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"

typedef BrowserWithTestWindowTest ToolbarModelTest;

TEST_F(ToolbarModelTest, ShouldDisplayURL) {
  browser()->OpenURL(GURL("view-source:http://www.google.com"),
                     GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(browser()->toolbar_model()->ShouldDisplayURL());
  EXPECT_EQ(ASCIIToUTF16("view-source:www.google.com"),
            browser()->toolbar_model()->GetText());

  browser()->OpenURL(GURL("view-source:chrome://newtab/"),
                     GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(browser()->toolbar_model()->ShouldDisplayURL());
  EXPECT_EQ(ASCIIToUTF16("view-source:chrome://newtab"),
            browser()->toolbar_model()->GetText());

  browser()->OpenURL(GURL("chrome-extension://monkey/balls.html"),
                     GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(browser()->toolbar_model()->ShouldDisplayURL());
  EXPECT_EQ(ASCIIToUTF16(""), browser()->toolbar_model()->GetText());

  browser()->OpenURL(GURL("chrome://newtab/"),
                     GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(browser()->toolbar_model()->ShouldDisplayURL());
  EXPECT_EQ(ASCIIToUTF16(""), browser()->toolbar_model()->GetText());

  browser()->OpenURL(GURL("about:blank"),
                     GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  EXPECT_TRUE(browser()->toolbar_model()->ShouldDisplayURL());
  EXPECT_EQ(ASCIIToUTF16("about:blank"),
            browser()->toolbar_model()->GetText());
}
