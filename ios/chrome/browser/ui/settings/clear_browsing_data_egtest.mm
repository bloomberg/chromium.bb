// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;

@interface ClearBrowsingDataSettingsTestCase : ChromeTestCase
@end

@implementation ClearBrowsingDataSettingsTestCase

- (void)openClearBrowsingDataDialog {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuSettingsId)]
      performAction:grey_tap()];
  NSString* settingsLabel =
      l10n_util::GetNSString(IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(settingsLabel)]
      performAction:grey_tap()];

  NSString* clearBrowsingDataDialogLabel =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          clearBrowsingDataDialogLabel)]
      performAction:grey_tap()];
}

- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Test that opening the clear browsing data dialog does not cause a crash.
- (void)testOpeningClearBrowsingData {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* oldSetting =
      [defaults stringForKey:@"EnableNewClearBrowsingDataUI"];
  [defaults setObject:@"Enabled" forKey:@"EnableNewClearBrowsingDataUI"];

  [self openClearBrowsingDataDialog];
  [self exitSettingsMenu];

  [defaults setObject:oldSetting forKey:@"EnableNewClearBrowsingDataUI"];
}

@end
