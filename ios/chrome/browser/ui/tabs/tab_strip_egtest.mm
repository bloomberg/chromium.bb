// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for the tab strip shown on iPad.
@interface TabStripTestCase : ChromeTestCase
@end

@implementation TabStripTestCase

// Test switching tabs using the tab strip.
- (void)testTabStripSwitchTabs {
  // Only iPad has a tab strip.
  if (IsCompact()) {
    return;
  }

  // TODO(crbug.com/238112):  Make this test also handle the 'collapsed' tab
  // case.
  const int kNumberOfTabs = 3;
  for (int i = 0; i < kNumberOfTabs - 1; i++) {
    [ChromeEarlGreyUI openNewTab];
  }

  // Note that the tab ordering wraps.  E.g. if A, B, and C are open,
  // and C is the current tab, the 'next' tab is 'A'.
  for (int i = 0; i < kNumberOfTabs + 1; i++) {
    GREYAssertTrue(chrome_test_util::GetMainTabCount() > 1,
                   chrome_test_util::GetMainTabCount() ? @"Only one tab open."
                                                       : @"No more tabs.");
    Tab* nextTab = chrome_test_util::GetNextTab();
    TabView* tabView = chrome_test_util::GetTabViewForTab(nextTab);

    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_ancestor(grey_equalTo(tabView)),
                                     grey_accessibilityID(@"Favicon"), nil)]
        performAction:grey_tap()];

    Tab* newCurrentTab = chrome_test_util::GetCurrentTab();
    GREYAssertTrue(newCurrentTab == nextTab,
                   @"The selected tab did not change to the next tab.");
  }
}

@end
