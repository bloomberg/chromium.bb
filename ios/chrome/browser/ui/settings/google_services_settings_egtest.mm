// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;
using chrome_test_util::GetOriginalBrowserState;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsDoneButton;
using unified_consent::prefs::kUnifiedConsentGiven;

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : ChromeTestCase

@property(nonatomic, strong) id<GREYMatcher> scrollViewMatcher;

@end

@implementation GoogleServicesSettingsTestCase

@synthesize scrollViewMatcher = _scrollViewMatcher;

// Opens the Google services settings view, and closes it.
- (void)testOpenGoogleServicesSettings {
  [self openGoogleServicesSettings];

  // Assert title and accessibility.
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the Google Services settings, when the user is not logged in.
// The personalized section is expect to be collapsed and the non-personalized
// section is expected to be expanded.
- (void)testOpeningServicesWhileSignedOut {
  [self openGoogleServicesSettings];
  [self assertPersonalizedServicesCollapsed:YES];
  [self assertNonPersonalizedServicesCollapsed:NO];
}

// Tests the Google Services settings, when the user is logged in without user
// consent.
// The "Sync Everything" section is expected to be visible.
// The personalized section and the non-personalized section are expected to be
// expanded.
- (void)testOpeningServicesWhileSignedIn {
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  [self resetUnifiedConsent];
  [self openGoogleServicesSettings];
  [self assertSyncEverythingSection];
  [self assertPersonalizedServicesCollapsed:NO];
  [self assertNonPersonalizedServicesCollapsed:NO];
}

// Tests the Google Services settings, when the user is logged in with user
// consent.
// The "Sync Everything" section is expected to be visible.
// The personalized section and the non-personalized section are expected to be
// collapsed.
- (void)testOpeningServicesWhileSignedInAndConsentGiven {
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  PrefService* prefService = GetOriginalBrowserState()->GetPrefs();
  GREYAssert(prefService->GetBoolean(kUnifiedConsentGiven),
             @"Unified consent should be given");
  [self openGoogleServicesSettings];
  [self assertSyncEverythingSection];
  [self assertPersonalizedServicesCollapsed:YES];
  [self assertNonPersonalizedServicesCollapsed:YES];
}

// Tests to expand/collapse the personalized section.
- (void)testTogglePersonalizedServices {
  [self openGoogleServicesSettings];
  [self assertPersonalizedServicesCollapsed:YES];
  [self togglePersonalizedServicesSection];
  [self assertPersonalizedServicesCollapsed:NO];
  [self scrollUp];
  [self togglePersonalizedServicesSection];
  [self assertPersonalizedServicesCollapsed:YES];
}

// Tests to expand/collapse the non-personalized section.
- (void)testToggleNonPersonalizedServices {
  [self openGoogleServicesSettings];
  [self assertNonPersonalizedServicesCollapsed:NO];
  [self scrollUp];
  [self toggleNonPersonalizedServicesSection];
  [self assertNonPersonalizedServicesCollapsed:YES];
  [self toggleNonPersonalizedServicesSection];
  [self assertNonPersonalizedServicesCollapsed:NO];
}

// Tests the "Manage synced data" cell does nothing when the user is not signed
// in.
- (void)testOpenManageSyncedDataWebPage {
  [self resetUnifiedConsent];
  [self openGoogleServicesSettings];
  [self togglePersonalizedServicesSection];
  [[self cellElementInteractionWithTitleID:
             IDS_IOS_GOOGLE_SERVICES_SETTINGS_MANAGED_SYNC_DATA_TEXT
                              detailTextID:0] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
}

// Tests the "Manage synced data" cell closes the settings, and opens the web
// page, while the user is signed in without user consent.
- (void)testOpenManageSyncedDataWebPageWhileSignedIn {
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  [self resetUnifiedConsent];
  [self openGoogleServicesSettings];
  [[self cellElementInteractionWithTitleID:
             IDS_IOS_GOOGLE_SERVICES_SETTINGS_MANAGED_SYNC_DATA_TEXT
                              detailTextID:0] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_nil()];
}

// Tests that "Activity and Interactions" switch should be disabled when the
// "History" sync is off.
- (void)testActivityAndInteractionsDisabledWithHistoryDisabled {
  // TODO(crbug.com/906680): Re-enable this test when it's fixed.
  EARL_GREY_TEST_DISABLED(@"Test disabled on simulators.");

  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  [self resetUnifiedConsent];
  [self openGoogleServicesSettings];
  // "Activity and Interactions" is enabled.
  [self
      assertSwitchCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_TEXT
                     detailTextID:
                         IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_DETAIL
                          enabled:YES];
  [self scrollUp];
  // Turn off "History".
  [[self cellElementInteractionWithTitleID:
             IDS_IOS_GOOGLE_SERVICES_SETTINGS_HISTORY_TEXT
                              detailTextID:0] performAction:grey_tap()];
  [self scrollUp];
  // "Activity and Interactions" is disabled.
  [self
      assertSwitchCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_TEXT
                     detailTextID:
                         IDS_IOS_GOOGLE_SERVICES_SETTINGS_ACTIVITY_AND_INTERACTIONS_DETAIL
                          enabled:NO];
}

#pragma mark - Helpers

// Resets the unified consent given by the user.
- (void)resetUnifiedConsent {
  PrefService* prefService = GetOriginalBrowserState()->GetPrefs();
  prefService->SetBoolean(kUnifiedConsentGiven, false);
}

// Opens the Google services settings.
- (void)openGoogleServicesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  self.scrollViewMatcher =
      grey_accessibilityID(@"google_services_settings_view_controller");
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
}

// Scrolls Google services settings to the top.
- (void)scrollUp {
  [[EarlGrey selectElementWithMatcher:self.scrollViewMatcher]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.1f, 0.1f)];
}

// Toggles personalized services section.
- (void)togglePersonalizedServicesSection {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(GetNSString(
              IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_PERSONALIZATION_TEXT))]
      performAction:grey_tap()];
}

// Toggles non personalized services section.
- (void)toggleNonPersonalizedServicesSection {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityLabel(GetNSString(
              IDS_IOS_GOOGLE_SERVICES_SETTINGS_NON_PERSONALIZED_SERVICES_TEXT))]
      performAction:grey_tap()];
}

// Returns grey matcher for a cell with |titleID| and |detailTextID|.
- (id<GREYMatcher>)cellMatcherWithTitleID:(int)titleID
                             detailTextID:(int)detailTextID {
  NSString* accessibilityLabel = GetNSString(titleID);
  if (detailTextID) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                   GetNSString(detailTextID)];
  }
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
                    grey_kindOfClass([UICollectionViewCell class]),
                    grey_sufficientlyVisible(), nil);
}

// Returns GREYElementInteraction for |matcher|, with a scroll down action.
- (GREYElementInteraction*)elementInteractionWithGreyMatcher:
    (id<GREYMatcher>)matcher {
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:self.scrollViewMatcher];
}

// Returns GREYElementInteraction for a cell based on the title string ID and
// the detail text string ID. |detailTextID| should be set to 0 if it doesn't
// exist in the cell.
- (GREYElementInteraction*)cellElementInteractionWithTitleID:(int)titleID
                                                detailTextID:(int)detailTextID {
  id<GREYMatcher> cellMatcher =
      [self cellMatcherWithTitleID:titleID detailTextID:detailTextID];
  return [self elementInteractionWithGreyMatcher:cellMatcher];
}

// Asserts that a cell exists, based on its title string ID and its detail text
// string ID. |detailTextID| should be set to 0 if it doesn't exist in the cell.
- (void)assertCellWithTitleID:(int)titleID detailTextID:(int)detailTextID {
  [[self cellElementInteractionWithTitleID:titleID detailTextID:detailTextID]
      assertWithMatcher:grey_notNil()];
}

// Asserts that the switch is enabled/disabled inside a cell with |titleID| and
// |detailTextID|.
- (void)assertSwitchCellWithTitleID:(int)titleID
                       detailTextID:(int)detailTextID
                            enabled:(BOOL)enabled {
  id<GREYMatcher> cellMatcher =
      [self cellMatcherWithTitleID:titleID detailTextID:detailTextID];
  id<GREYMatcher> enabledMatcher = grey_enabled();
  if (!enabled) {
    enabledMatcher = grey_not(grey_enabled());
  }
  id<GREYMatcher> switchMatcher =
      grey_allOf(enabledMatcher, grey_kindOfClass([UISwitch class]),
                 grey_ancestor(cellMatcher), nil);
  GREYElementInteraction* element =
      [self elementInteractionWithGreyMatcher:switchMatcher];
  [element assertWithMatcher:grey_notNil()];
}

// Asserts that the sync everthing section cell is visible.
- (void)assertSyncEverythingSection {
  [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_EVERYTHING
                 detailTextID:0];
}

// Asserts that the personalized service section is visible and collapsed or
// expended.
- (void)assertPersonalizedServicesCollapsed:(BOOL)collapsed {
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
    [self assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_SETTINGS_TEXT
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

// Asserts that the non-personalized service section is visible and collapsed or
// expended.
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
