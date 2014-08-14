// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"

// There are spoken feedback tests in browser/chromeos/accessibility but
// these tests rely on browser views, so they have been placed in this file
// in the views directory. The shared function between the spoken feedback
// tests that simulates the touch screen with ChromeVox is kept in the
// accessiblity utils file.
typedef InProcessBrowserTest SpokenFeedbackViewsTest;

namespace chromeos {

// Fetch the bookmark bar instructions view from the browser and trigger an
// accessibilty event on the view to ensure the correct text is being read.
IN_PROC_BROWSER_TEST_F(SpokenFeedbackViewsTest,
                       TouchExploreBookmarkBarInstructions) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  SpeechMonitor monitor;
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(monitor.SkipChromeVoxEnabledMessage());

  accessibility::SimulateTouchScreenInChromeVoxForTest(browser()->profile());

  // Send an accessibility hover event on the empty bookmarks bar, which is
  // what we get when you tap it on a touch screen when ChromeVox is on.
  BrowserView* browser_view =
      static_cast<BrowserView*>(browser()->window());
  BookmarkBarView* bookmarks_bar = browser_view->GetBookmarkBarView();
  views::View* instructions = bookmarks_bar->GetBookmarkBarInstructionsView();
  instructions->NotifyAccessibilityEvent(ui::AX_EVENT_HOVER, true);

  EXPECT_EQ("Bookmarks,", monitor.GetNextUtterance());
  EXPECT_EQ("For quick access,", monitor.GetNextUtterance());
  EXPECT_EQ("place your bookmarks here on the bookmarks bar.",
            monitor.GetNextUtterance());
}

// Fetch a tab view from the browser and trigger an accessibilty event on the
// view to ensure the correct text is being read.
IN_PROC_BROWSER_TEST_F(SpokenFeedbackViewsTest, TouchExploreTabs) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  SpeechMonitor monitor;
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(monitor.SkipChromeVoxEnabledMessage());

  accessibility::SimulateTouchScreenInChromeVoxForTest(browser()->profile());

  // Send an accessibility hover event on a tab in the tap strip, which is
  // what we get when you tap it on a touch screen when ChromeVox is on.
  BrowserView* browser_view =
      static_cast<BrowserView*>(browser()->window());
  TabStrip* tab_strip = browser_view->tabstrip();
  ASSERT_EQ(tab_strip->tab_count(), 1);

  Tab* tab = tab_strip->tab_at(0);
  tab->NotifyAccessibilityEvent(ui::AX_EVENT_HOVER, true);
  EXPECT_TRUE(MatchPattern(monitor.GetNextUtterance(), "*, tab * of *"));
}

}  // namespace chromeos
