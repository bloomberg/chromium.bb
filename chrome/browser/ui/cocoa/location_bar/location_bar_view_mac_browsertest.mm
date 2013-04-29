// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#import "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/search_token_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/separator_decoration.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/browser/ui/toolbar/test_toolbar_model.h"

class LocationBarViewMacBrowserTest : public InProcessBrowserTest {
 public:
  LocationBarViewMacBrowserTest() : InProcessBrowserTest(),
                                    old_toolbar_model_(NULL) {
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    old_toolbar_model_ = GetLocationBar()->toolbar_model_;
    GetLocationBar()->toolbar_model_ = &test_toolbar_model_;
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    GetLocationBar()->toolbar_model_ = old_toolbar_model_;
  }

  LocationBarViewMac* GetLocationBar() const {
    BrowserWindowController* controller =
        [BrowserWindowController browserWindowControllerForWindow:
            browser()->window()->GetNativeWindow()];
    return [controller locationBarBridge];
  }

  SearchTokenDecoration* GetSearchTokenDecoration() const {
    return GetLocationBar()->search_token_decoration_.get();
  }

  SeparatorDecoration* GetSeparatorDecoration() const {
    return GetLocationBar()->separator_decoration_.get();
  }

 protected:
  TestToolbarModel test_toolbar_model_;

 private:
  ToolbarModel* old_toolbar_model_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMacBrowserTest);
};

// Verify that the search token decoration is displayed when there are search
// terms in the omnibox.
IN_PROC_BROWSER_TEST_F(LocationBarViewMacBrowserTest, SearchToken) {
  GetLocationBar()->Layout();
  EXPECT_FALSE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_FALSE(GetSeparatorDecoration()->IsVisible());

  test_toolbar_model_.SetInputInProgress(false);
  test_toolbar_model_.set_search_terms_type(ToolbarModel::NORMAL_SEARCH_TERMS);
  GetLocationBar()->Layout();
  EXPECT_TRUE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_TRUE(GetSeparatorDecoration()->IsVisible());

  test_toolbar_model_.SetInputInProgress(true);
  GetLocationBar()->Layout();
  EXPECT_FALSE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_FALSE(GetSeparatorDecoration()->IsVisible());
}

// Verify that the prominent search token UI is displayed.
IN_PROC_BROWSER_TEST_F(LocationBarViewMacBrowserTest, ProminentSearchToken) {
  GetLocationBar()->Layout();
  EXPECT_FALSE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_FALSE(GetSeparatorDecoration()->IsVisible());

  test_toolbar_model_.SetInputInProgress(false);
  test_toolbar_model_.set_search_terms_type(
      ToolbarModel::URL_LIKE_SEARCH_TERMS);
  GetLocationBar()->Layout();
  EXPECT_TRUE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_FALSE(GetSeparatorDecoration()->IsVisible());

  test_toolbar_model_.SetInputInProgress(true);
  GetLocationBar()->Layout();
  EXPECT_FALSE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_FALSE(GetSeparatorDecoration()->IsVisible());
}
