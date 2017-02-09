// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/wait_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NewTabPageController (ExposedForTesting)
- (GoogleLandingController*)googleLandingController {
  return googleLandingController_;
}
@end

@interface GoogleLandingController (ExposedForTesting)
- (BOOL)scrolledToTop;
- (BOOL)animateHeader;
@end

namespace {

void DismissNewTabPagePanel() {
  if (!IsIPadIdiom()) {
    id<GREYMatcher> matcher = grey_allOf(grey_accessibilityID(@"Exit"),
                                         grey_sufficientlyVisible(), nil);
    [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  }
}

// Pauses until the history label has disappeared.  History should not show on
// incognito.
void WaitForHistoryToDisappear() {
  [[GREYCondition
      conditionWithName:@"Wait for history to disappear"
                  block:^BOOL {
                    NSError* error = nil;
                    NSString* history =
                        l10n_util::GetNSString(IDS_HISTORY_SHOW_HISTORY);
                    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                            history)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:testing::kWaitForUIElementTimeout];
}

// Displays the |panel_type| new tab page.  An a phone this will send a command
// to display a dialog, on tablet this calls -selectPanel to slide the NTP.
void SelectNewTabPagePanel(NewTabPage::PanelIdentifier panel_type) {
  NewTabPageController* ntp_controller =
      chrome_test_util::GetCurrentNewTabPageController();
  if (IsIPadIdiom()) {
    [ntp_controller selectPanel:panel_type];
  } else {
    NSUInteger tag = 0;
    if (panel_type == NewTabPage::PanelIdentifier::kBookmarksPanel) {
      tag = IDC_SHOW_BOOKMARK_MANAGER;
    } else if (panel_type == NewTabPage::PanelIdentifier::kOpenTabsPanel) {
      tag = IDC_SHOW_OTHER_DEVICES;
    }
    if (tag) {
      GenericChromeCommand* command =
          [[GenericChromeCommand alloc] initWithTag:tag];
      chrome_test_util::RunCommandWithActiveViewController(command);
    }
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

void AssertNTPScrolledToTop(bool scrolledToTop) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  NewTabPageController* ntp_controller =
      chrome_test_util::GetCurrentNewTabPageController();
  GoogleLandingController* google_landing_controller =
      [ntp_controller googleLandingController];
  [[GREYCondition
      conditionWithName:@"Wait for end of animation."
                  block:^BOOL {
                    return ![google_landing_controller animateHeader];
                  }] waitWithTimeout:testing::kWaitForUIElementTimeout];
  GREYAssertTrue([google_landing_controller scrolledToTop] == scrolledToTop,
                 @"scrolledToTop_ does not match expected value");
}

}  // namespace

@interface UIWindow (Hidden)
- (UIResponder*)firstResponder;
@end

@interface NewTabPageTestCase : ChromeTestCase
@end

@implementation NewTabPageTestCase

// Tests that all items are accessible on the most visited page.
- (void)testAccessibilityOnMostVisited {
  SelectNewTabPagePanel(NewTabPage::kMostVisitedPanel);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all items are accessible on the open tabs page.
- (void)testAccessibilityOnOpenTabs {
  SelectNewTabPagePanel(NewTabPage::kOpenTabsPanel);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  DismissNewTabPagePanel();
}

// Tests that all items are accessible on the bookmarks page.
- (void)testAccessibilityOnBookmarks {
  SelectNewTabPagePanel(NewTabPage::kBookmarksPanel);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  DismissNewTabPagePanel();
}

// Tests that all items are accessible on the incognito page.
- (void)testAccessibilityOnIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
  SelectNewTabPagePanel(NewTabPage::kIncognitoPanel);
  WaitForHistoryToDisappear();
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  chrome_test_util::CloseAllIncognitoTabs();
}

// Tests rotating the device when the NTP's omnibox is pinned to the top of the
// screen.
- (void)testRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  NSString* ntpOmniboxLabel = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  NSString* focusedOmniboxLabel = l10n_util::GetNSString(IDS_ACCNAME_LOCATION);
  SelectNewTabPagePanel(NewTabPage::kMostVisitedPanel);
  AssertNTPScrolledToTop(NO);

  if (IsIPadIdiom()) {
    // Tap in omnibox to scroll it to the top of the screen.
    id<GREYMatcher> matcher = grey_accessibilityLabel(ntpOmniboxLabel);
    [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  } else {
    // Swipe up the NTP. The omnibox should be pinned at the top of the screen.
    id<GREYMatcher> matcher = grey_accessibilityID(@"Google Landing");
    [[EarlGrey selectElementWithMatcher:matcher]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  }
  AssertNTPScrolledToTop(YES);

  // Restore the orientation of the device to portrait in Teardown.
  [self setTearDownHandler:^{
    if ([UIDevice currentDevice].orientation != UIDeviceOrientationPortrait) {
      [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                               errorOrNil:nil];
    }
  }];

  // Rotate to landscape and check that the device and the status bar are
  // rotated properly and the omnibox is still scrolled up.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];
  GREYAssertTrue(
      [UIDevice currentDevice].orientation == UIDeviceOrientationLandscapeLeft,
      @"Device orientation should now be left landscape");
  GREYAssertTrue([[UIApplication sharedApplication] statusBarOrientation] ==
                     UIInterfaceOrientationLandscapeRight,
                 @"Status bar orientation should now be right landscape");

  AssertNTPScrolledToTop(YES);

  // Tap in the omnibox.
  NSString* omniboxLabel =
      IsIPadIdiom() ? focusedOmniboxLabel : ntpOmniboxLabel;
  id<GREYMatcher> matcher = grey_allOf(grey_accessibilityLabel(omniboxLabel),
                                       grey_minimumVisiblePercent(0.2), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  // Rotate to portrait and check that the device and the status bar are rotated
  // properly and the omnibox is still scrolled up.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  GREYAssertTrue(
      [UIDevice currentDevice].orientation == UIDeviceOrientationPortrait,
      @"Device orientation should now be portrait");
  GREYAssertTrue([[UIApplication sharedApplication] statusBarOrientation] ==
                     UIInterfaceOrientationPortrait,
                 @"Status bar orientation should now be portrait");

  AssertNTPScrolledToTop(YES);

  // Check that omnibox is still focused.
  UIResponder* firstResponder =
      [[UIApplication sharedApplication].keyWindow firstResponder];
  BOOL equal =
      [[firstResponder accessibilityLabel] isEqualToString:focusedOmniboxLabel];
  GREYAssertTrue(
      equal,
      @"Expected accessibility label for first responder to be '%@', got '%@'",
      focusedOmniboxLabel, [firstResponder accessibilityLabel]);
}

// Tests focusing and defocusing the NTP's omnibox.
- (void)testOmnibox {
  // Empty the pasteboard: if it contains a link the Google Landing will not be
  // interactable.
  [UIPasteboard generalPasteboard].string = @"";

  NSString* omniboxLabel = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);
  if (IsIPadIdiom()) {
    SelectNewTabPagePanel(NewTabPage::kMostVisitedPanel);
  }

  // Check that the NTP is in its normal state.
  id<GREYMatcher> omnibox_matcher =
      grey_allOf(grey_accessibilityLabel(omniboxLabel),
                 grey_minimumVisiblePercent(0.2), nil);
  [[EarlGrey selectElementWithMatcher:omnibox_matcher]
      assertWithMatcher:grey_notNil()];
  AssertNTPScrolledToTop(NO);

  // Tap in omnibox to scroll it to the top of the screen.
  [[EarlGrey selectElementWithMatcher:omnibox_matcher]
      performAction:grey_tap()];

  // Check that the omnibox is scrolled to the top of the screen and its cancel
  // button is visible.
  AssertNTPScrolledToTop(YES);

  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(cancelLabel)]
        assertWithMatcher:grey_notNil()];
  }

  // Swipe down in the NTP to return to the normal state.
  id<GREYMatcher> landing_matcher = grey_accessibilityID(@"Google Landing");
  [[EarlGrey selectElementWithMatcher:landing_matcher]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  AssertNTPScrolledToTop(NO);
  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(cancelLabel)]
        assertWithMatcher:grey_notVisible()];
  }

  // Tap in omnibox and check that it's scrolled to the top of the screen and
  // its cancel button is visible.
  [[EarlGrey selectElementWithMatcher:omnibox_matcher]
      performAction:grey_tap()];
  AssertNTPScrolledToTop(YES);

  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(cancelLabel)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
  // Tap below the omnibox to cancel editing and return to the normal state.
  [[EarlGrey selectElementWithMatcher:landing_matcher]
      performAction:grey_tap()];
  AssertNTPScrolledToTop(NO);

  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(cancelLabel)]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests that the NTP's toolbar is not visible when the NTP is scrolled up.
- (void)testScrollToolbar {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no hidden toolbar in tablet)");
  }

  NSString* tabSwitcherLabel =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TABS);
  NSString* toolsMenuLabel = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);

  // Check that the toolbar's tab switcher and tools menu buttons are visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(tabSwitcherLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(toolsMenuLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
  AssertNTPScrolledToTop(NO);

  // Swipe up the NTP. The omnibox should be fixed at the top of the screen.
  id<GREYMatcher> matcher = grey_accessibilityID(@"Google Landing");
  [[EarlGrey selectElementWithMatcher:matcher]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  AssertNTPScrolledToTop(YES);

  // Check that tab switcher and tools menu buttons are not on screen.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(tabSwitcherLabel)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(toolsMenuLabel)]
      assertWithMatcher:grey_notVisible()];
}

@end
