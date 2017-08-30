// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"

typedef BrowserWithTestWindowTest SearchDelegateTest;

// Test the propagation of search "mode" changes from the tab's search model to
// the browser's search model.
TEST_F(SearchDelegateTest, SearchModel) {
  // Initial state.
  EXPECT_EQ(SearchModel::Origin::DEFAULT, browser()->search_model()->origin());

  // Propagate change from tab's search model to browser's search model.
  AddTab(browser(), GURL("http://foo/0"));
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  SearchTabHelper::FromWebContents(first_tab)->model()->SetOrigin(
      SearchModel::Origin::NTP);
  EXPECT_EQ(SearchModel::Origin::NTP, browser()->search_model()->origin());

  // Add second tab (it gets inserted at index 0), make it active, and make sure
  // its mode changes propagate to the browser's search model.
  AddTab(browser(), GURL("http://foo/1"));
  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_NE(first_tab, second_tab);
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ(SearchModel::Origin::DEFAULT, browser()->search_model()->origin());
  SearchTabHelper::FromWebContents(second_tab)
      ->model()
      ->SetOrigin(SearchModel::Origin::NTP);
  EXPECT_EQ(SearchModel::Origin::NTP, browser()->search_model()->origin());

  // The first tab is not active so changes should not propagate.
  SearchTabHelper::FromWebContents(first_tab)->model()->SetOrigin(
      SearchModel::Origin::DEFAULT);
  EXPECT_EQ(SearchModel::Origin::NTP, browser()->search_model()->origin());
}
