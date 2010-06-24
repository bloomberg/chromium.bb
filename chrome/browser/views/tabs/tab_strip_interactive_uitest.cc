// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/tabs/base_tab.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

typedef InProcessBrowserTest TabStripTest;

// Creates a tab, middle clicks to close the first, then clicks back on the
// first. This test exercises a crasher in the mouse near code path.
IN_PROC_BROWSER_TEST_F(TabStripTest, Close) {
  int initial_tab_count = browser()->tab_count();

  browser()->AddTabWithURL(
      GURL(chrome::kAboutBlankURL), GURL(), PageTransition::TYPED, -1,
      TabStripModel::ADD_SELECTED, NULL, std::string());
  views::RootView* root =
      views::Widget::FindRootView(browser()->window()->GetNativeHandle());
  ASSERT_TRUE(root);
  TabStrip* tab_strip =
      static_cast<TabStrip*>(root->GetViewByID(VIEW_ID_TAB_STRIP));
  ASSERT_TRUE(tab_strip);

  // Force a layout to ensure no animations are active.
  tab_strip->Layout();

  // Close the first tab by way of am middle click.
  BaseTab* tab1 = tab_strip->base_tab_at_tab_index(1);
  ui_controls::MoveMouseToCenterAndPress(
      tab1, ui_controls::MIDDLE, ui_controls::DOWN | ui_controls::UP,
      new MessageLoop::QuitTask());

  // Force a layout to ensure no animations are active.
  ui_test_utils::RunMessageLoop();

  EXPECT_EQ(initial_tab_count, browser()->tab_count());

  // Force a layout to ensure no animations are active.
  tab_strip->Layout();

  // Click on the first tab.
  BaseTab* tab0 = tab_strip->base_tab_at_tab_index(0);
  ui_controls::MoveMouseToCenterAndPress(
      tab0, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      new MessageLoop::QuitTask());

  ui_test_utils::RunMessageLoop();
}
