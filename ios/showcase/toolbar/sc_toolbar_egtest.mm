// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

// Tests for the toolbar view controller.
@interface SCToolbarTestCase : ShowcaseTestCase
@end

@implementation SCToolbarTestCase

- (void)setUp {
  [super setUp];
  Open(@"ToolbarViewController");
}

- (void)tearDown {
  Close();
  if ([UIDevice currentDevice].orientation != UIDeviceOrientationPortrait) {
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                             errorOrNil:nil];
  }
  [super tearDown];
}

// Tests if the Toolbar buttons have the right accessibility labels and
// commands.
- (void)testVerifyToolbarButtonsLabelAndAction {
  // Buttons displayed in both Regular and Compact SizeClasses.
  // Back Button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   l10n_util::GetNSString(IDS_ACCNAME_BACK))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"goBack")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // Forward Button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   l10n_util::GetNSString(IDS_ACCNAME_FORWARD))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"goForward")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // ShowTabStrip Button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_TOOLBAR_SHOW_TABS))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"showTabStrip")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // Menu Button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_TOOLBAR_SETTINGS))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"showToolsMenu")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // Buttons displayed only in Regular SizeClass.
  if (!IsCompact()) {
    // Share Button.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_TOOLS_MENU_SHARE))]
        assertWithMatcher:grey_notNil()];

    // Reload Button.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ACCNAME_RELOAD))]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"reloadPage")]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                            @"protocol_alerter_done")]
        performAction:grey_tap()];

    // Stop Button.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_ACCNAME_STOP))]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(@"stopLoadingPage")]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                            @"protocol_alerter_done")]
        performAction:grey_tap()];
  }
}

// Tests that the Menu will be closed on rotation from portrait to landscape and
// viceversa.
- (void)testRotation {
  // TODO(crbug.com/652464): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  // Rotate from portrait to landscape.
  if ([EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                               errorOrNil:nil]) {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(@"closeToolsMenu")]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                            @"protocol_alerter_done")]
        performAction:grey_tap()];

    // If successful rotate back from landscape to portrait.
    if ([EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                                 errorOrNil:nil]) {
      [[EarlGrey
          selectElementWithMatcher:grey_accessibilityLabel(@"closeToolsMenu")]
          assertWithMatcher:grey_notNil()];
      [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                              @"protocol_alerter_done")]
          performAction:grey_tap()];
    }
  }
}

// Tests that the Regular SizeClass buttons appear when a rotation causes a
// SizeClass change from Compact to Regular. E.g. iPhone Plus rotation from
// portrait to landscape and viceversa.
- (void)testRotationSizeClassChange {
  // TODO(crbug.com/652464): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  // If currently on compact rotate to check if SizeClass changes to regular.
  if (IsCompact()) {
    if ([EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                 errorOrNil:nil]) {
      [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                              @"protocol_alerter_done")]
          performAction:grey_tap()];
      // If SizeClass is not compact after rotation, confirm that some
      // ToolbarButtons are now visible.
      if (!IsCompact()) {
        [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                l10n_util::GetNSString(
                                                    IDS_IOS_TOOLS_MENU_SHARE))]
            assertWithMatcher:grey_notNil()];
        [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                l10n_util::GetNSString(
                                                    IDS_IOS_ACCNAME_RELOAD))]
            assertWithMatcher:grey_notNil()];
        [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                l10n_util::GetNSString(
                                                    IDS_IOS_ACCNAME_STOP))]
            assertWithMatcher:grey_notNil()];
        // Going back to Compact Width.
        if ([EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                                     errorOrNil:nil]) {
          [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                  @"protocol_alerter_done")]
              performAction:grey_tap()];
          [[EarlGrey
              selectElementWithMatcher:grey_accessibilityLabel(
                                           l10n_util::GetNSString(
                                               IDS_IOS_TOOLS_MENU_SHARE))]
              assertWithMatcher:grey_nil()];
          [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                  l10n_util::GetNSString(
                                                      IDS_IOS_ACCNAME_RELOAD))]
              assertWithMatcher:grey_nil()];
          [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                  l10n_util::GetNSString(
                                                      IDS_IOS_ACCNAME_STOP))]
              assertWithMatcher:grey_nil()];
        }
      }
    }
  }
}

@end
