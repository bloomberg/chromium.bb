// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

#import "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/search_token_decoration.h"
#import "chrome/browser/ui/cocoa/location_bar/separator_decoration.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

// Override the toolbar model to allow |GetInputInProgress| and
// |WouldReplaceSearchURLWithSearchTerms| to be customized.
class MockToolbarModel : public ToolbarModel {
 public:
  MockToolbarModel() : ToolbarModel(),
                       input_in_progress_(false),
                       would_replace_search_url_with_search_terms_(false) {}
  virtual ~MockToolbarModel() {}

  virtual string16 GetText(
      bool display_search_urls_as_search_terms) const OVERRIDE {
    return string16();
  }
  virtual GURL GetURL() const OVERRIDE { return GURL(); }
  virtual bool WouldReplaceSearchURLWithSearchTerms() const OVERRIDE {
    return would_replace_search_url_with_search_terms_;
  }
  virtual SecurityLevel GetSecurityLevel() const OVERRIDE { return NONE; }
  virtual int GetIcon() const OVERRIDE { return 0; }
  virtual string16 GetEVCertName() const OVERRIDE { return string16(); }
  virtual bool ShouldDisplayURL() const OVERRIDE { return false; }
  virtual void SetInputInProgress(bool value) OVERRIDE {
    input_in_progress_ = value;
  }
  virtual bool GetInputInProgress() const OVERRIDE {
    return input_in_progress_;
  }

  void set_would_replace_search_url_with_search_terms(bool value) {
    would_replace_search_url_with_search_terms_ = value;
  }

 private:
  bool input_in_progress_;
  bool would_replace_search_url_with_search_terms_;
};

}  // namespace

class LocationBarViewMacBrowserTest : public InProcessBrowserTest {
 public:
  LocationBarViewMacBrowserTest() : InProcessBrowserTest(),
                                    old_toolbar_model_(NULL) {
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    old_toolbar_model_ = GetLocationBar()->toolbar_model_;
    GetLocationBar()->toolbar_model_ = &mock_toolbar_model_;
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
  MockToolbarModel mock_toolbar_model_;

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

  mock_toolbar_model_.SetInputInProgress(false);
  mock_toolbar_model_.set_would_replace_search_url_with_search_terms(true);
  GetLocationBar()->Layout();
  EXPECT_TRUE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_TRUE(GetSeparatorDecoration()->IsVisible());

  mock_toolbar_model_.SetInputInProgress(true);
  GetLocationBar()->Layout();
  EXPECT_FALSE(GetSearchTokenDecoration()->IsVisible());
  EXPECT_FALSE(GetSeparatorDecoration()->IsVisible());
}
