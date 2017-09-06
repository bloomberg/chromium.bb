// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

namespace {
// Matcher for the Delete button on the bookmarks UI.
id<GREYMatcher> BookmarksDeleteSwipeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_DELETE);
}

// Matcher for the Back button on the bookmarks UI.
id<GREYMatcher> BookmarksBackButton() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BACK_LABEL));
}

// Matcher for the DONE button on the bookmarks UI.
id<GREYMatcher> BookmarksDoneButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitKeyboardKey)), nil);
}

// Matcher for context bar leading button.
id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(@"context_bar_leading_button"),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for context bar center button.
id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(@"context_bar_center_button"),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for context bar trailing button.
id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(@"context_bar_trailing_button"),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}
}  // namespace

// Bookmark integration tests for Chrome.
@interface BookmarksNewGenTestCase : ChromeTestCase
@end

@implementation BookmarksNewGenTestCase

- (void)setUp {
  [super setUp];
  // Wait for bookmark model to be loaded.
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout,
                 ^{
                   return chrome_test_util::BookmarksLoaded();
                 }),
             @"Bookmark model did not load");
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tear down called once per test.
- (void)tearDown {
  [super tearDown];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

#pragma mark - Tests

- (void)testUndoDeleteBookmarkFromSwipe {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksNewGenTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
}

// Tests that the bookmark context bar is shown in MobileBookmarks.
- (void)testBookmarkContextBarShown {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"context_bar")]
      assertWithMatcher:grey_notNil()];

  // Verify the context bar's leading and trailing buttons are shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_leading_button")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      assertWithMatcher:grey_notNil()];
}

- (void)testBookmarkContextBarInVariousSelectionModes {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"context_bar")]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Select single URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Select single Folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "Edit" enabled "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarEditString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Multi select URL and folders.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect Folder 1, so that Second URL is selected.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "More" enabled
  // "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all, but one Folder - Folder 1 is selected.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];
  // Unselect URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];

  // Verify context bar shows enabled "Delete" enabled "Edit" enabled "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarEditString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Unselect all.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify context bar shows disabled "Delete" disabled "More" enabled
  // "Cancel".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  // Cancel edit mode
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      performAction:grey_tap()];

  // Verify context bar shows enabled "New Folder" and enabled "Select".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarSelectString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];
}

- (void)testContextMenuForSingleURLSelection {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_COPY)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextMenuForMultipleURLSelection {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextMenuForSingleFolderSelection {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Center button is "Edit".
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarEditString])]
      assertWithMatcher:grey_allOf(grey_notNil(), grey_enabled(), nil)];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Edit menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarEditString])]
      performAction:grey_tap()];

  // Verify it shows edit view controller.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Editor")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextMenuForMultipleFolderSelection {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select Folders.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1.1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextMenuForMixedSelection {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Try deleting a bookmark from the edit screen, then undoing that delete.
- (void)testUndoDeleteBookmarkFromEditScreen {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select Folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap edit on context bar.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarEditString])]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_GROUP_DELETE)]
      performAction:grey_tap()];

  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for bookmark to go away");

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify Delete is disabled.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)testDeleteSingleURLNode {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksNewGenTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify Delete is disabled.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];

  // Cancel edit mode.
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      performAction:grey_tap()];
}

- (void)testDeleteSingleFolderNode {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select single URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksNewGenTestCase waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Verify Delete is disabled.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled),
                                   nil)];

  // Cancel edit mode.
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      performAction:grey_tap()];
}

- (void)testDeleteMultipleNodes {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select Folder and URL.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksNewGenTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];
  [BookmarksNewGenTestCase waitForDeletionOfBookmarkWithTitle:@"Folder 1"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_notNil()];

  // Cancel edit mode.
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      performAction:grey_tap()];
}

// Tests that the promo view is only seen at root level and not in any of the
// child nodes.
- (void)testPromoViewIsSeenOnlyInRootNode {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarksNewGenTestCase setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Go to child node.
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Wait until promo is gone.
  [SigninEarlGreyUtils checkSigninPromoNotVisible];

  // Check that the promo already seen state is not updated.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];

  // Come back to root node, and the promo view should appear.
  [[EarlGrey selectElementWithMatcher:BookmarksBackButton()]
      performAction:grey_tap()];

  // Check promo view is still visible.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tapping No thanks on the promo make it disappear.
- (void)testPromoNoThanksMakeItDisappear {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarksNewGenTestCase setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSigninPromoCloseButtonId)]
      performAction:grey_tap()];

  // Wait until promo is gone.
  [SigninEarlGreyUtils checkSigninPromoNotVisible];

  // Check that the promo already seen state is updated.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:YES];
}

// Tests the tapping on the primary button of sign-in promo view in a cold
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithColdStateUsingPrimaryButton {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase openBookmarks];

  // Check that sign-in promo view are visible.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Cancel")]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
}

// Tests the tapping on the primary button of sign-in promo view in a warm
// state makes the confirmaiton sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingPrimaryButton {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];

  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Check that promo is visible.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Tap the Sign in button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoSecondaryButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap the CANCEL button.
  [[EarlGrey selectElementWithMatcher:
                 grey_buttonTitle([l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)
                     uppercaseString])] performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
}

// Tests the tapping on the secondary button of sign-in promo view in a warm
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingSecondaryButton {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Check that sign-in promo view are visible.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  // Tap the secondary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap the CANCEL button.
  [[EarlGrey selectElementWithMatcher:
                 grey_buttonTitle([l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)
                     uppercaseString])] performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
}

// Tests that the sign-in promo should not be shown after been shown 19 times.
- (void)testAutomaticSigninPromoDismiss {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, 19);
  [BookmarksNewGenTestCase openBookmarks];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  // Check the sign-in promo will not be shown anymore.
  [BookmarksNewGenTestCase verifyPromoAlreadySeen:YES];
  GREYAssertEqual(
      20, prefs->GetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount),
      @"Should have incremented the display count");
  // Close the bookmark view and open it again.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
  [BookmarksNewGenTestCase openBookmarks];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUtils checkSigninPromoNotVisible];
}

#pragma mark - Helpers

// Navigates to the bookmark manager UI.
+ (void)openBookmarks {
  [ChromeEarlGreyUI openToolsMenu];

  // Opens the bookmark manager.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuBookmarksId)]
      performAction:grey_tap()];

  // Wait for it to load, and the menu to go away.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuBookmarksId)]
      assertWithMatcher:grey_nil()];
}

// Selects |bookmarkFolder| to open.
+ (void)openBookmarkFolder:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(
                                       NSClassFromString(@"UITableViewCell")),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)] performAction:grey_tap()];
}

// Loads a set of default bookmarks in the model for the tests to use.
+ (void)setupStandardBookmarks {
  [BookmarksNewGenTestCase waitForBookmarkModelLoaded:YES];

  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  const GURL firstURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  NSString* firstTitle = @"First URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(firstTitle), firstURL);

  const GURL secondURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  NSString* secondTitle = @"Second URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(secondTitle), secondURL);

  NSString* folderTitle = @"Folder 1";
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      bookmark_model->mobile_node(), 0, base::SysNSStringToUTF16(folderTitle));
  folderTitle = @"Folder 1.1";
  bookmark_model->AddFolder(bookmark_model->mobile_node(), 0,
                            base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 2";
  const bookmarks::BookmarkNode* folder2 = bookmark_model->AddFolder(
      folder1, 0, base::SysNSStringToUTF16(folderTitle));

  folderTitle = @"Folder 3";
  const bookmarks::BookmarkNode* folder3 = bookmark_model->AddFolder(
      folder2, 0, base::SysNSStringToUTF16(folderTitle));

  const GURL thirdURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/chromium_logo_page.html");
  NSString* thirdTitle = @"Third URL";
  bookmark_model->AddURL(folder3, 0, base::SysNSStringToUTF16(thirdTitle),
                         thirdURL);
}

// Selects MobileBookmarks to open.
+ (void)openMobileBookmarks {
  [BookmarksNewGenTestCase openBookmarkFolder:@"Mobile Bookmarks"];
}

// Checks that the promo has already been seen or not.
+ (void)verifyPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosBookmarkPromoAlreadySeen) == seen) {
    return;
  }
  NSString* errorDesc = (seen)
                            ? @"Expected promo already seen, but it wasn't."
                            : @"Expected promo not already seen, but it was.";
  GREYFail(errorDesc);
}

// Checks that the promo has already been seen or not.
+ (void)setPromoAlreadySeen:(BOOL)seen {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browserState->GetPrefs();
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, seen);
}

// Waits for the disparition of the given |title| in the UI.
+ (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(title)]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for bookmark to go away");
}

// Waits for the bookmark model to be loaded in memory.
+ (void)waitForBookmarkModelLoaded:(BOOL)loaded {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout,
                 ^{
                   return bookmarkModel->loaded() == loaded;
                 }),
             @"Bookmark model was not loaded");
}

// Context bar strings.
+ (NSString*)contextBarNewFolderString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER);
}

+ (NSString*)contextBarDeleteString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE);
}

+ (NSString*)contextBarCancelString {
  return l10n_util::GetNSString(IDS_CANCEL);
}

+ (NSString*)contextBarSelectString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_SELECT);
}

+ (NSString*)contextBarEditString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_EDIT);
}

+ (NSString*)contextBarMoreString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
}
// TODO(crbug.com/695749): Add egtest for spinner and empty background

@end
