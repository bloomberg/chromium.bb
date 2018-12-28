// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "components/prefs/pref_service.h"
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
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the Google Services settings.
- (void)testOpeningServices {
  [self openGoogleServicesSettings];
  [self assertNonPersonalizedServices];
}

#pragma mark - Helpers

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

// Asserts that the non-personalized service section is visible.
- (void)assertNonPersonalizedServices {
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_AUTOCOMPLETE_SEARCHES_AND_URLS_DETAIL];
  [self
      assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_PRELOAD_PAGES_DETAIL];
  [self
      assertCellWithTitleID:IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_IMPROVE_CHROME_DETAIL];
  [self
      assertCellWithTitleID:
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT
               detailTextID:
                   IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_DETAIL];
}

@end
