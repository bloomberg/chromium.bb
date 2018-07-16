// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsDoneButton;

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : ChromeTestCase

@property(nonatomic, strong) id<GREYMatcher> scollViewMatcher;

@end

@implementation GoogleServicesSettingsTestCase

@synthesize scollViewMatcher = _scollViewMatcher;

// Opens the Google services settings view, and closes it.
- (void)testOpenGoogleServicesSettings {
  if (!IsUIRefreshPhase1Enabled())
    EARL_GREY_TEST_SKIPPED(@"This test is UIRefresh only.");
  [self openGoogleServicesSettings];

  // Assert title and accessibility.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"google_services_settings_view_controller")]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests if the Google Services settings contains only the non personalized
// options when the user is not logged in.
- (void)testServicesWhileSignedOut {
  // TODO(crbug.com/863860): re-enable when fixed.
  if (!IsIPadIdiom())
    EARL_GREY_TEST_DISABLED(@"Fails on iPhones.");

  if (!IsUIRefreshPhase1Enabled())
    EARL_GREY_TEST_SKIPPED(@"This test is UIRefresh only.");
  [self openGoogleServicesSettings];
  [self assertSyncPersonalizedServicesCollapsed:YES];
  [self assertNonPersonalizedServicesCollapsed:NO];
  [self toggleNonPersonalizedServicesSection];
  [self assertNonPersonalizedServicesCollapsed:YES];
  [self toggleNonPersonalizedServicesSection];
  [self assertNonPersonalizedServicesCollapsed:NO];
}

#pragma mark - Helpers

- (void)openGoogleServicesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  self.scollViewMatcher =
      grey_accessibilityID(@"google_services_settings_view_controller");
  [[EarlGrey selectElementWithMatcher:self.scollViewMatcher]
      assertWithMatcher:grey_notNil()];
}

- (void)toggleNonPersonalizedServicesSection {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(GetNSString(
              IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_TEXT))]
      performAction:grey_tap()];
}

- (void)assertCellWithTitleID:(int)titleID detailTextID:(int)detailTextID {
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(GetNSString(titleID))]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
      onElementWithMatcher:self.scollViewMatcher]
      assertWithMatcher:grey_notNil()];
  if (detailTextID) {
    [[[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                             GetNSString(detailTextID))]
           usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
        onElementWithMatcher:self.scollViewMatcher]
        assertWithMatcher:grey_notNil()];
  }
}

- (void)assertSyncEverythingSection {
  [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_EVERYTHING
                 detailTextID:0];
}

- (void)assertSyncPersonalizedServicesCollapsed:(BOOL)collapsed {
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_DETAIL];
  if (!collapsed) {
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_BOOKMARKS_TEXT
                   detailTextID:0];
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_HISTORY_TEXT
                   detailTextID:0];
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_PASSWORD_TEXT
                   detailTextID:0];
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_OPENTABS_TEXT
                   detailTextID:0];
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOFILL_TEXT
                   detailTextID:0];
    [self
        assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_READING_LIST_TEXT
                 detailTextID:0];
    [self
        assertCellWithTitleID:
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_TEXT
                 detailTextID:
                     IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_DETAIL];
    [self
        assertCellWithTitleID:
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_GOOGLE_ACTIVITY_CONTROL_TEXT
                 detailTextID:
                     IDS_IOS_GOOGLE_SERVICES_SETTINGS_GOOGLE_ACTIVITY_CONTROL_DETAIL];
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_ENCRYPTION_TEXT
                   detailTextID:0];
    [self assertCellWithTitleID:
              IDS_IOS_GOOGLE_SERVICES_SETTINGS_MANAGED_SYNC_DATA_TEXT
                   detailTextID:0];
  }
}

- (void)assertNonPersonalizedServicesCollapsed:(BOOL)collapsed {
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_DETAIL];
  if (!collapsed) {
    [self
        assertCellWithTitleID:
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
                 detailTextID:
                     IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL];
    [self assertCellWithTitleID:
              IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_TEXT
                   detailTextID:
                       IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_DETAIL];
    [self assertCellWithTitleID:
              IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
                   detailTextID:
                       IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
    [self
        assertCellWithTitleID:
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
                 detailTextID:
                     IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
  }
}

@end
