// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_bubble_links_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"

typedef InProcessBrowserTest OneClickSigninBubbleLinksDelegateBrowserTest;

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleLinksDelegateBrowserTest,
                       AdvancedLink) {
  scoped_ptr<OneClickSigninBubbleDelegate> delegate_;
  delegate_.reset(new OneClickSigninBubbleLinksDelegate(browser()));

  int starting_tab_count = browser()->tab_strip_model()->count();

  // The current tab should be replaced.
  delegate_->OnAdvancedLinkClicked();

  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count, tab_count);
}

IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleLinksDelegateBrowserTest,
                       LearnMoreLink) {
  scoped_ptr<OneClickSigninBubbleDelegate> delegate_;
  delegate_.reset(new OneClickSigninBubbleLinksDelegate(browser()));

  int starting_tab_count = browser()->tab_strip_model()->count();

  // A new tab should be opened.
  delegate_->OnLearnMoreLinkClicked(false);

  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}
