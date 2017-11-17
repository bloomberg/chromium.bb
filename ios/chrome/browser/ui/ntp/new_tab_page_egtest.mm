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

}  // namespace

@interface NewTabPageTestCase : ChromeTestCase
@end

@implementation NewTabPageTestCase

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

@end
