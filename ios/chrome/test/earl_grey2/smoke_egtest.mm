// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TestLib/EarlGreyImpl/EarlGrey.h>
#import <UIKit/UIKit.h>

#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey2/chrome_earl_grey_edo.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that string resources are loaded into the ResourceBundle and available
// for use in tests.
- (void)testAppResourcesArePresent {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];

  NSString* settingsLabel = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(settingsLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap a second time to close the menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Tests that helpers in chrome_earl_grey_ui.h are available for use in tests.
- (void)testReload {
  [ChromeEarlGreyUI reload];
}

// Tests navigation-related converted helpers in chrome_earl_grey.h.
- (void)testURLNavigation {
  [ChromeEarlGrey loadURL:GURL("chrome://terms")];
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
}

// Tests tab open/close-related converted helpers in chrome_earl_grey.h.
- (void)testTabOpeningAndClosing {
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey closeAllTabsInCurrentMode];
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey openNewTab];
}

@end
