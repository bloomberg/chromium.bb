// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/aura/window.h"

namespace {

typedef InProcessBrowserTest LocationIconViewTest;

// Verify that clicking the location icon a second time hides the bubble.
IN_PROC_BROWSER_TEST_F(LocationIconViewTest, HideOnSecondClick) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* location_icon_view =
      browser_view->GetToolbarView()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);

  // Verify that clicking once shows the location icon bubble.
  scoped_refptr<content::MessageLoopRunner> runner1 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndPress(
      location_icon_view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      runner1->QuitClosure());
  runner1->Run();

  EXPECT_TRUE(WebsiteSettingsPopupView::IsPopupShowing());

  // Verify that clicking again doesn't reshow it.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndPress(
      location_icon_view,
      ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP,
      runner2->QuitClosure());
  runner2->Run();

  EXPECT_FALSE(WebsiteSettingsPopupView::IsPopupShowing());
}

}  // namespace
