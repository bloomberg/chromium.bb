// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TestLib/EarlGreyImpl/EarlGrey.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey2/chrome_earl_grey_edo.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case to verify that EarlGrey tests can be launched and perform basic
// UI interactions.
@interface Eg2TestCase : BaseEarlGreyTestCase
@end

@implementation Eg2TestCase

// Tests that a tab can be opened.
- (void)testOpenTab {
  // Open tools menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  // Open new tab.
  // TODO(crbug.com/917114): Calling the string directly is temporary while we
  // roll out a solution to access constants across the code base for EG2.
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(@"kToolsMenuNewTabId");
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];

  // Get tab count.
  NSUInteger tabCount =
      [[GREYHostApplicationDistantObject sharedInstance] GetMainTabCount];
  GREYAssertEqual(2, tabCount, @"Expected 2 tabs.");
}

// Tests that helpers from chrome_matchers.h are available for use in tests.
- (void)testTapToolsMenu {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  // Tap a second time to close the menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that helpers from chrome_actions.h are available for use in tests.
- (void)testToggleSettingsSwitch {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuPasswordsButton()]
      performAction:grey_tap()];

  // Toggle the passwords switch off and on.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"savePasswordsItem_switch")]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"savePasswordsItem_switch")]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Close the settings menu.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that helpers from chrome_earl_grey.h are available for use in tests.
- (void)testClearBrowsingHistory {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey clearBrowsingHistory]);
}

@end
