// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/browser_list/browser_list.h"

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser_list_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class BrowserListTestObserver : public BrowserListObserver {
 public:
  BrowserListTestObserver() = default;

  // Reset statistics whether events has been called.
  void ResetStatistics() {
    browser_created_called_ = false;
    browser_removed_called_ = false;
  }

  // Returns whether OnBrowserCreated() was invoked.
  bool browser_created_called() const { return browser_created_called_; }

  // Returns whether OnBrowserRemoved() was invoked.
  bool browser_removed_called() const { return browser_removed_called_; }

  // BrowserListObserver implementation.
  void OnBrowserCreated(BrowserList* browser_list, Browser* browser) override {
    browser_created_called_ = true;
  }

  void OnBrowserRemoved(BrowserList* browser_list, Browser* browser) override {
    browser_removed_called_ = true;
  }

 private:
  bool browser_created_called_ = false;
  bool browser_removed_called_ = false;
};

}  // namespace

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
  EXPECT_EQ(0, browser_list->count());

  Browser* browser = browser_list->CreateNewBrowser();
  ASSERT_EQ(1, browser_list->count());
  EXPECT_EQ(browser, browser_list->GetBrowserAtIndex(0));
}

TEST_F(BrowserListTest, CloseBrowser) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());
  browser_list->CreateNewBrowser();
  EXPECT_EQ(1, browser_list->count());

  browser_list->CloseBrowserAtIndex(0);
  EXPECT_EQ(0, browser_list->count());
}

TEST_F(BrowserListTest, ContainsIndex) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());
  EXPECT_EQ(0, browser_list->count());
  EXPECT_FALSE(browser_list->ContainsIndex(-1));
  EXPECT_FALSE(browser_list->ContainsIndex(0));
  EXPECT_FALSE(browser_list->ContainsIndex(1));

  browser_list->CreateNewBrowser();
  EXPECT_EQ(1, browser_list->count());
  EXPECT_FALSE(browser_list->ContainsIndex(-1));
  EXPECT_TRUE(browser_list->ContainsIndex(0));
  EXPECT_FALSE(browser_list->ContainsIndex(1));

  browser_list->CreateNewBrowser();
  EXPECT_EQ(2, browser_list->count());
  EXPECT_FALSE(browser_list->ContainsIndex(-1));
  EXPECT_TRUE(browser_list->ContainsIndex(0));
  EXPECT_TRUE(browser_list->ContainsIndex(1));
}

TEST_F(BrowserListTest, GetIndexOfBrowser) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());
  EXPECT_EQ(BrowserList::kInvalidIndex,
            browser_list->GetIndexOfBrowser(nullptr));

  Browser* browser_0 = browser_list->CreateNewBrowser();
  EXPECT_EQ(0, browser_list->GetIndexOfBrowser(browser_0));
  EXPECT_EQ(browser_0, browser_list->GetBrowserAtIndex(0));

  Browser* browser_1 = browser_list->CreateNewBrowser();
  EXPECT_EQ(1, browser_list->GetIndexOfBrowser(browser_1));
  EXPECT_EQ(browser_1, browser_list->GetBrowserAtIndex(1));

  // browser_0 is now invalid.
  browser_list->CloseBrowserAtIndex(0);
  EXPECT_EQ(BrowserList::kInvalidIndex,
            browser_list->GetIndexOfBrowser(browser_0));
  EXPECT_EQ(0, browser_list->GetIndexOfBrowser(browser_1));
  EXPECT_EQ(browser_1, browser_list->GetBrowserAtIndex(0));
}

TEST_F(BrowserListTest, Observer) {
  BrowserList* browser_list = BrowserList::FromBrowserState(browser_state());

  BrowserListTestObserver observer;
  ScopedObserver<BrowserList, BrowserListObserver> scoped_observer(&observer);
  scoped_observer.Add(browser_list);

  observer.ResetStatistics();
  ASSERT_FALSE(observer.browser_created_called());
  ASSERT_FALSE(observer.browser_removed_called());

  browser_list->CreateNewBrowser();
  EXPECT_TRUE(observer.browser_created_called());
  EXPECT_FALSE(observer.browser_removed_called());

  observer.ResetStatistics();
  ASSERT_FALSE(observer.browser_created_called());
  ASSERT_FALSE(observer.browser_removed_called());

  browser_list->CloseBrowserAtIndex(0);
  EXPECT_FALSE(observer.browser_created_called());
  EXPECT_TRUE(observer.browser_removed_called());
}
