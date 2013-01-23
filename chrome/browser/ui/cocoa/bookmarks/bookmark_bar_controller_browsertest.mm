// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "googleurl/src/gurl.h"

class BottomBookmarkBarControllerTest : public InProcessBrowserTest {
 public:
  BottomBookmarkBarControllerTest() {
    chrome::search::EnableInstantExtendedAPIForTesting();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    AddTabAtIndex(1,
                  GURL(chrome::kChromeUINewTabURL),
                  content::PAGE_TRANSITION_LINK);
    EXPECT_TRUE([GetBookmarkBarController() shouldShowAtBottomWhenDetached]);
    EXPECT_FALSE([[GetBookmarkBarController() view] isHidden]);
  }

  BrowserWindowController* GetBrowserWindowController() const {
    NSWindow* window = browser()->window()->GetNativeWindow();
    return [BrowserWindowController browserWindowControllerForWindow:window];
  }

  BookmarkBarController* GetBookmarkBarController() const {
    return [GetBrowserWindowController() bookmarkBarController];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BottomBookmarkBarControllerTest);
};

// Verify that the bookmark bar is positioned correctly when detached and
// attached.
IN_PROC_BROWSER_TEST_F(BottomBookmarkBarControllerTest, Position) {
  // Verify detached bookmark bar is at the bottom of content area.
  NSRect bookmark_bar_frame = [[GetBookmarkBarController() view] frame];
  EXPECT_EQ(0, NSMinY(bookmark_bar_frame));

  // Attach the bookmark bar and verify it's above the content area.
  browser()->window()->ToggleBookmarkBar();
  content::RunAllPendingInMessageLoop();
  NSRect content_frame = [[GetBrowserWindowController() tabContentArea] frame];
  bookmark_bar_frame = [[GetBookmarkBarController() view] frame];
  EXPECT_EQ(NSMinX(content_frame), NSMinX(bookmark_bar_frame));
  EXPECT_EQ(NSMaxY(content_frame), NSMinY(bookmark_bar_frame));
}

// Test that detached bookmark bar is positioned correctly when the download
// shelf is visible.
IN_PROC_BROWSER_TEST_F(BottomBookmarkBarControllerTest, PositionWithDownloads) {
  // Show the download shelf.
  DownloadShelfController* download_shelf_controller =
      [GetBrowserWindowController() downloadShelf];
  [download_shelf_controller show:nil];
  EXPECT_FALSE([[download_shelf_controller view] isHidden]);

  // Verify that the bookmark bar is at the bottom of the content area but above
  // the download shelf.
  NSRect content_frame = [[GetBrowserWindowController() tabContentArea] frame];
  NSRect bookmark_bar_frame = [[GetBookmarkBarController() view] frame];
  EXPECT_EQ(NSMinY(content_frame), NSMinY(bookmark_bar_frame));
  EXPECT_NE(0, NSMinY(bookmark_bar_frame));
  EXPECT_EQ(NSMinY(content_frame), NSMinY(bookmark_bar_frame));
}

// Verify that the bookmark bar shrinks when the window is narrow.
IN_PROC_BROWSER_TEST_F(BottomBookmarkBarControllerTest, NarrowWindow) {
  NSWindow* window = browser()->window()->GetNativeWindow();
  NSRect window_frame = [window frame];
  CGFloat expected_width =
      roundf(chrome::search::kMaxWidthForBottomBookmarkBar / 2.0);
  window_frame.size.width = expected_width +
      chrome::search::kHorizontalPaddingForBottomBookmarkBar * 2;
  [window setFrame:window_frame display:YES];
  EXPECT_FALSE([[GetBookmarkBarController() view] isHidden]);

  NSRect frame = [[GetBookmarkBarController() view] frame];
  EXPECT_EQ(chrome::search::kHorizontalPaddingForBottomBookmarkBar,
            NSMinX(frame));
  EXPECT_EQ(expected_width, NSWidth(frame));
}

// Verify that the bookmark bar's width doesn't go above its maximum value.
IN_PROC_BROWSER_TEST_F(BottomBookmarkBarControllerTest, WideWindow) {
  NSWindow* window = browser()->window()->GetNativeWindow();
  NSRect window_frame = [window frame];
  window_frame.size.width = chrome::search::kMaxWidthForBottomBookmarkBar +
      chrome::search::kHorizontalPaddingForBottomBookmarkBar * 2 + 100;
  [window setFrame:window_frame display:YES];
  EXPECT_FALSE([[GetBookmarkBarController() view] isHidden]);

  NSRect frame = [[GetBookmarkBarController() view] frame];
  EXPECT_EQ(roundf((NSWidth(window_frame) - NSWidth(frame)) / 2.0),
            NSMinX(frame));
  EXPECT_EQ(chrome::search::kMaxWidthForBottomBookmarkBar, NSWidth(frame));
}

// Verify that the bookmark bar is hidden if the content area height falls
// below a minimum value.
IN_PROC_BROWSER_TEST_F(BottomBookmarkBarControllerTest, MinContentHeight) {
  NSWindow* window = browser()->window()->GetNativeWindow();
  NSRect window_frame = [window frame];
  window_frame.size.height =
      chrome::search::kMinContentHeightForBottomBookmarkBar;
  [window setFrame:window_frame display:YES];
  EXPECT_TRUE([[GetBookmarkBarController() view] isHidden]);
}

IN_PROC_BROWSER_TEST_F(BottomBookmarkBarControllerTest, NoItemContainer) {
  EXPECT_TRUE([GetBookmarkBarController() isEmpty]);
  BookmarkBarView* button_view = [GetBookmarkBarController() buttonView];
  EXPECT_TRUE([[button_view noItemContainer] isHidden]);

  // Add a bookmark.
  BookmarkModel* model =
      BookmarkModelFactory::GetForProfile(browser()->profile());
  const BookmarkNode* node = model->bookmark_bar_node();
  model->AddURL(node,
                node->child_count(),
                ASCIIToUTF16("title"),
                GURL("http://www.google.com"));

  // Item container should remain hidden.
  EXPECT_FALSE([GetBookmarkBarController() isEmpty]);
  EXPECT_TRUE([[button_view noItemContainer] isHidden]);
}
