// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/browser_list/browser_list.h"

#include <memory>

#include "base/macros.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class BrowserListTest : public PlatformTest {
 public:
  BrowserListTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }

 private:
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;

  DISALLOW_COPY_AND_ASSIGN(BrowserListTest);
};

TEST_F(BrowserListTest, Initialisation) {
  EXPECT_TRUE(BrowserList::FromBrowserState(browser_state()));
}

TEST_F(BrowserListTest, CreateBrowser) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());
  EXPECT_EQ(0, browser_list->GetBrowserCount());

  Browser* browser = browser_list->CreateNewBrowser();
  ASSERT_EQ(1, browser_list->GetBrowserCount());
  EXPECT_EQ(browser, browser_list->GetBrowserAtIndex(0));
}

TEST_F(BrowserListTest, CloseBrowser) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());
  browser_list->CreateNewBrowser();
  EXPECT_EQ(1, browser_list->GetBrowserCount());

  browser_list->CloseBrowserAtIndex(0);
  EXPECT_EQ(0, browser_list->GetBrowserCount());
}

TEST_F(BrowserListTest, ContainsIndex) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());
  EXPECT_EQ(0, browser_list->GetBrowserCount());
  EXPECT_FALSE(browser_list->ContainsIndex(-1));
  EXPECT_FALSE(browser_list->ContainsIndex(0));
  EXPECT_FALSE(browser_list->ContainsIndex(1));

  browser_list->CreateNewBrowser();
  EXPECT_EQ(1, browser_list->GetBrowserCount());
  EXPECT_FALSE(browser_list->ContainsIndex(-1));
  EXPECT_TRUE(browser_list->ContainsIndex(0));
  EXPECT_FALSE(browser_list->ContainsIndex(1));

  browser_list->CreateNewBrowser();
  EXPECT_EQ(2, browser_list->GetBrowserCount());
  EXPECT_FALSE(browser_list->ContainsIndex(-1));
  EXPECT_TRUE(browser_list->ContainsIndex(0));
  EXPECT_TRUE(browser_list->ContainsIndex(1));
}
