// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/range/range.h"

typedef BrowserWithTestWindowTest BookmarkBubbleSignInDelegateTest;

TEST_F(BookmarkBubbleSignInDelegateTest, OnSignInLinkClicked) {
  int starting_tab_count = browser()->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  delegate->OnSignInLinkClicked();

  // A new tab should have been opened.
  int tab_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
}

// Verifies that the sign in page can be loaded in a different browser
// if the provided browser is invalidated.
TEST_F(BookmarkBubbleSignInDelegateTest, BrowserRemoved) {
  // Create an extra browser.
  scoped_ptr<BrowserWindow> extra_window;
  extra_window.reset(CreateBrowserWindow());

  Browser::CreateParams params(browser()->profile(),
                               browser()->host_desktop_type());
  params.window = extra_window.get();
  scoped_ptr<Browser> extra_browser;
  extra_browser.reset(new Browser(params));

  int starting_tab_count = extra_browser->tab_strip_model()->count();

  scoped_ptr<BookmarkBubbleDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser()));

  BrowserList::SetLastActive(extra_browser.get());

  browser()->tab_strip_model()->CloseAllTabs();
  set_browser(NULL);

  delegate->OnSignInLinkClicked();

  // A new tab should have been opened in the extra browser.
  int tab_count = extra_browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);

  // Required to avoid a crash when the browser is deleted.
  extra_browser->tab_strip_model()->CloseAllTabs();
}
