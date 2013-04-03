// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/views/focus/focus_manager.h"

using views::FocusManager;

typedef InProcessBrowserTest BrowserViewTest;

IN_PROC_BROWSER_TEST_F(BrowserViewTest, BrowserView) {
  BookmarkBarView::DisableAnimationsForTesting(true);

  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  TopContainerView* top_container = browser_view->top_container();

  // Verify the view hierarchy.
  EXPECT_EQ(top_container, browser_view->tabstrip()->parent());
  EXPECT_EQ(top_container, browser_view->toolbar()->parent());
  EXPECT_EQ(top_container, browser_view->GetBookmarkBarView()->parent());
  EXPECT_EQ(browser_view, browser_view->infobar_container()->parent());

  // Top container is at the front of the view hierarchy.
  EXPECT_EQ(browser_view->child_count() - 1,
            browser_view->GetIndexOf(top_container));

  // Verify basic layout.
  EXPECT_EQ(0, top_container->x());
  EXPECT_EQ(0, top_container->y());
  EXPECT_EQ(browser_view->width(), top_container->width());

  // Verify bookmark bar visibility.
  BookmarkBarView* bookmark_bar = browser_view->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());

  // Bookmark bar is reparented to BrowserView on NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_TRUE(bookmark_bar->visible());
  EXPECT_TRUE(bookmark_bar->IsDetached());
  EXPECT_EQ(browser_view, bookmark_bar->parent());
  // Top container is still in front.
  EXPECT_EQ(browser_view->child_count() - 1,
            browser_view->GetIndexOf(top_container));

  // Bookmark bar is parented back to top container on normal page.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_FALSE(bookmark_bar->visible());
  EXPECT_FALSE(bookmark_bar->IsDetached());
  EXPECT_EQ(top_container, bookmark_bar->parent());
  // Top container is still in front.
  EXPECT_EQ(browser_view->child_count() - 1,
            browser_view->GetIndexOf(top_container));

  BookmarkBarView::DisableAnimationsForTesting(false);
}

// Active window and focus testing is not reliable on Windows crbug.com/79493
// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))
#define MAYBE_FullscreenClearsFocus DISABLED_FullscreenClearsFocus
#else
#define MAYBE_FullscreenClearsFocus FullscreenClearsFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_FullscreenClearsFocus) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  FocusManager* focus_manager = browser_view->GetFocusManager();

  // Focus starts in the location bar or one of its children.
  EXPECT_TRUE(location_bar_view->Contains(focus_manager->GetFocusedView()));

  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Focus is released from the location bar.
  EXPECT_FALSE(location_bar_view->Contains(focus_manager->GetFocusedView()));
}
