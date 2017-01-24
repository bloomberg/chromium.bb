// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

namespace {
// Displacement for scroll action.
const CGFloat kScrollDisplacement = 50.0;
}  // namespace
@interface TranslateUITestCase : ChromeTestCase
@end

@implementation TranslateUITestCase

// Opens the translate settings page and verifies that accessibility is set up
// properly.
- (void)testAccessibilityOfTranslateSettings {
  // Open translate settings.
  // TODO(crbug.com/606815): This and close settings is mostly shared with block
  // popups settings tests, and others. See if this can move to shared code.
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuSettingsId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:grey_accessibilityID(kToolsMenuTableViewId)]
      performAction:grey_tap()];
  [[[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SETTINGS_TITLE)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:grey_accessibilityID(kSettingsCollectionViewId)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_TRANSLATE_SETTING)]
      performAction:grey_tap()];

  // Assert title and accessibility.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"translate_settings_view_controller")]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Close settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
}

@end
