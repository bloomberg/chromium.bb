// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/modal_ntp.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleLandingViewController (ExposedForTesting)
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
void SelectNewTabPagePanel(ntp_home::PanelIdentifier panel_type) {
  NewTabPageController* ntp_controller =
      chrome_test_util::GetCurrentNewTabPageController();
  if (IsIPadIdiom()) {
    [ntp_controller selectPanel:panel_type];
  } else if (panel_type == ntp_home::BOOKMARKS_PANEL) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC()
        showBookmarksManager];
  } else if (panel_type == ntp_home::RECENT_TABS_PANEL) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() showRecentTabs];
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

void AssertNTPScrolledToTop(bool scrolledToTop) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  NewTabPageController* ntp_controller =
      chrome_test_util::GetCurrentNewTabPageController();
  GoogleLandingViewController* google_landing_view_controller =
      [ntp_controller googleLandingController];
  [[GREYCondition
      conditionWithName:@"Wait for end of animation."
                  block:^BOOL {
                    return ![google_landing_view_controller animateHeader];
                  }] waitWithTimeout:testing::kWaitForUIElementTimeout];
  GREYAssertTrue(
      [google_landing_view_controller scrolledToTop] == scrolledToTop,
      @"scrolledToTop_ does not match expected value");
}

}  // namespace

@interface UIWindow (Hidden)
- (UIResponder*)firstResponder;
@end

@interface NewTabPageTestCase : ChromeTestCase {
  std::unique_ptr<base::test::ScopedCommandLine> _scopedCommandLine;
}
@end

@implementation NewTabPageTestCase

- (void)setUp {
  // The command line is set up before [super setUp] in order to have the NTP
  // opened with the command line already setup.
  _scopedCommandLine = std::make_unique<base::test::ScopedCommandLine>();
  base::CommandLine* commandLine = _scopedCommandLine->GetProcessCommandLine();
  commandLine->AppendSwitch(switches::kDisableSuggestionsUI);
  [super setUp];
}

- (void)tearDown {
  _scopedCommandLine.reset();
}

#pragma mark - Tests

// Tests that all items are accessible on the most visited page.
- (void)testAccessibilityOnMostVisited {
  SelectNewTabPagePanel(ntp_home::HOME_PANEL);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all items are accessible on the open tabs page.
- (void)testAccessibilityOnOpenTabs {
  SelectNewTabPagePanel(ntp_home::RECENT_TABS_PANEL);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  DismissNewTabPagePanel();
}

// Tests that all items are accessible on the bookmarks page.
- (void)testAccessibilityOnBookmarks {
  // TODO(crbug.com/782551): Rewrite this test for the new Bookmarks UI.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kBookmarkNewGeneration);

  SelectNewTabPagePanel(ntp_home::BOOKMARKS_PANEL);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  DismissNewTabPagePanel();
}

// Tests that all items are accessible on the incognito page.
- (void)testAccessibilityOnIncognitoTab {
  chrome_test_util::OpenNewIncognitoTab();
  SelectNewTabPagePanel(ntp_home::INCOGNITO_PANEL);
  WaitForHistoryToDisappear();
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  GREYAssert(chrome_test_util::CloseAllIncognitoTabs(), @"Tabs did not close");
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
  SelectNewTabPagePanel(ntp_home::HOME_PANEL);
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
  // TODO(crbug.com/782551): Rewrite this test for the new Bookmarks UI.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kBookmarkNewGeneration);

  // TODO(crbug.com/782551): Remove the following when this test is rewritten.
  // Re-open tabs so that scoped_feature_list will take effect on the NTP.
  chrome_test_util::CloseAllTabs();
  chrome_test_util::OpenNewTab();

  // Empty the pasteboard: if it contains a link the Google Landing will not be
  // interactable.
  [UIPasteboard generalPasteboard].string = @"";

  NSString* omniboxLabel = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  NSString* cancelLabel = l10n_util::GetNSString(IDS_CANCEL);
  if (IsIPadIdiom()) {
    SelectNewTabPagePanel(ntp_home::HOME_PANEL);
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

  NSString* toolsMenuLabel = l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS);

  // Check that the toolbar's tab switcher and tools menu buttons are visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(toolsMenuLabel)]
      assertWithMatcher:grey_notVisible()];
}

@end
