// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"

typedef BrowserWithTestWindowTest SearchDelegateTest;

// Test the propagation of search "mode" changes from the tab's search model to
// the browser's search model.
TEST_F(SearchDelegateTest, SearchModel) {
  chrome::EnableInstantExtendedAPIForTesting();

  // Initial state.
  EXPECT_TRUE(browser()->search_model()->mode().is_default());

  // Propagate change from tab's search model to browser's search model.
  AddTab(browser(), GURL("http://foo/0"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  SearchTabHelper::FromWebContents(web_contents)->model()->
      SetMode(SearchMode(SearchMode::MODE_NTP, SearchMode::ORIGIN_NTP));
  EXPECT_TRUE(browser()->search_model()->mode().is_ntp());

  // Add second tab, make it active, and make sure its mode changes
  // propagate to the browser's search model.
  AddTab(browser(), GURL("http://foo/1"));
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  SearchTabHelper::FromWebContents(web_contents)->model()->
      SetMode(SearchMode(SearchMode::MODE_SEARCH_RESULTS,
                         SearchMode::ORIGIN_DEFAULT));
  EXPECT_TRUE(browser()->search_model()->mode().is_search());

  // The first tab is not active so changes should not propagate.
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  SearchTabHelper::FromWebContents(web_contents)->model()->
      SetMode(SearchMode(SearchMode::MODE_NTP, SearchMode::ORIGIN_NTP));
  EXPECT_TRUE(browser()->search_model()->mode().is_search());
}
