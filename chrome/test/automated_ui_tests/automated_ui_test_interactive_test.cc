// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

TEST_F(AutomatedUITestBase, DragOut) {
  NewTab();
  NewTab();
  ASSERT_TRUE(active_browser()->
      WaitForTabCountToBecome(3, action_max_timeout_ms()));
  PlatformThread::Sleep(sleep_timeout_ms());
  ASSERT_TRUE(DragTabOut());
  int window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);
}

TEST_F(AutomatedUITestBase, DragLeftRight) {
  NewTab();
  NewTab();
  ASSERT_TRUE(active_browser()->
      WaitForTabCountToBecome(3, action_max_timeout_ms()));
  // TODO(phajdan.jr): We need a WaitForTabstripAnimationsToEnd() function.
  // Every sleep in this file should be replaced with it.
  PlatformThread::Sleep(sleep_timeout_ms());

  scoped_refptr<TabProxy> dragged_tab(active_browser()->GetActiveTab());
  int tab_index;
  ASSERT_TRUE(dragged_tab->GetTabIndex(&tab_index));
  EXPECT_EQ(2, tab_index);

  // Drag the active tab to left. Now it should be the middle tab.
  ASSERT_TRUE(DragActiveTab(false));
  // We wait for the animation to be over.
  PlatformThread::Sleep(sleep_timeout_ms());
  ASSERT_TRUE(dragged_tab->GetTabIndex(&tab_index));
  EXPECT_EQ(1, tab_index);

  // Drag the active tab to left. Now it should be the leftmost tab.
  ASSERT_TRUE(DragActiveTab(false));
  PlatformThread::Sleep(sleep_timeout_ms());
  ASSERT_TRUE(dragged_tab->GetTabIndex(&tab_index));
  EXPECT_EQ(0, tab_index);

  // Drag the active tab to left. It should fail since the active tab is
  // already the leftmost tab.
  ASSERT_FALSE(DragActiveTab(false));

  // Drag the active tab to right. Now it should be the middle tab.
  ASSERT_TRUE(DragActiveTab(true));
  PlatformThread::Sleep(sleep_timeout_ms());
  ASSERT_TRUE(dragged_tab->GetTabIndex(&tab_index));
  EXPECT_EQ(1, tab_index);

  // Drag the active tab to right. Now it should be the rightmost tab.
  ASSERT_TRUE(DragActiveTab(true));
  PlatformThread::Sleep(sleep_timeout_ms());
  ASSERT_TRUE(dragged_tab->GetTabIndex(&tab_index));
  EXPECT_EQ(2, tab_index);

  // Drag the active tab to right. It should fail since the active tab is
  // already the rightmost tab.
  ASSERT_FALSE(DragActiveTab(true));
}
