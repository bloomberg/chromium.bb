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
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
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
#include "ui/base/models/tree_node_iterator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

namespace {

// GURL for the testing bookmark "First URL".
const GURL getFirstURL() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
}

// GURL for the testing bookmark "Second URL".
const GURL getSecondURL() {
  return web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
}

// GURL for the testing bookmark "French URL".
const GURL getFrenchURL() {
  return web::test::HttpServer::MakeUrl("http://www.a.fr/");
}

// Matcher for the Delete button on the bookmarks UI.
id<GREYMatcher> BookmarksDeleteSwipeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_DELETE);
}

// Matcher for the Back button on the bookmarks UI.
id<GREYMatcher> BookmarksBackButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_BOOKMARK_NEW_BACK_LABEL)),
                    grey_enabled(), nil);
}

// Matcher for the DONE button on the bookmarks UI.
id<GREYMatcher> BookmarksDoneButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitKeyboardKey)),
      grey_sufficientlyVisible(), nil);
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

// Matcher for tappable bookmark node.
id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(label), grey_sufficientlyVisible(),
                    nil);
}
}  // namespace

// Bookmark integration tests for Chrome.
@interface BookmarksNewGenTestCase : ChromeTestCase
@end

@implementation BookmarksNewGenTestCase

- (void)setUp {
  // Set flags before setup.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);
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
  // Clear position cache so that Bookmarks starts at the root folder in next
  // test.
  bookmark_utils_ios::ClearBookmarkUIPositionCache();
}

#pragma mark - Tests

// Test that swiping left to right navigate back.
- (void)testNavigateBackWithGesture {
  // Disabled on iPad as there is not "navigate back" gesture.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPad");
  }

// TODO(crbug.com/768339): This test is faling on devices with iOS > 9 because
// grey_swipeFastInDirectionWithStartPoint does not work.
#if !TARGET_IPHONE_SIMULATOR
  if (@available(iOS 10.0, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 10+ devices.");
  }
#endif

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Make sure the Mobile Bookmarks is not present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mobile Bookmarks")]
      assertWithMatcher:grey_nil()];

  // Open the first folder, to be able to go back twice on the bookmarks.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Back twice using swipe left gesture.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionRight,
                                                            0.01, 0.5)];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionRight,
                                                            0.01, 0.5)];

  // Check we navigated back to the Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Mobile Bookmarks")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testUndoDeleteBookmarkFromSwipe {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify context bar does not change when "Delete" shows up.
  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];

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

  // Verify context bar remains in default state.
  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];
}

- (void)testSwipeToDeleteDisabledInEditMode {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button shows up.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_notNil()];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Verify the delete confirmation button is gone after entering edit mode.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_nil()];

  // Swipe action on "Second URL".  This should not bring out delete
  // confirmation button as swipe-to-delete is disabled in edit mode.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button doesn't appear.
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_nil()];

  // Cancel edit mode
  [BookmarksNewGenTestCase closeContextBarEditMode];

  // Swipe action on the URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"French URL")]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Verify the delete confirmation button shows up. (swipe-to-delete is
  // re-enabled).
  [[[EarlGrey selectElementWithMatcher:BookmarksDeleteSwipeButton()]
      inRoot:grey_kindOfClass(NSClassFromString(@"UITableView"))]
      assertWithMatcher:grey_notNil()];
}

// Tests that the bookmark context bar is shown in MobileBookmarks.
- (void)testBookmarkContextBarShown {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  // Unselect URL.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
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
  [BookmarksNewGenTestCase closeContextBarEditMode];

  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];
}

- (void)testContextMenuForSingleURLSelection {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [self verifyContextMenuForSingleURL];
}

// Verify Edit functionality on single URL selection.
- (void)testEditFunctionalityOnSingleURL {
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];

  // Invoke Edit through context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notNil()];

  // Edit URL.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Title Field_textField")]
      performAction:grey_replaceText(@"n5")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"URL Field_textField")]
      performAction:grey_replaceText(@"www.a.fr")];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notVisible()];

  // Verify that the bookmark was updated.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"n5")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [BookmarksNewGenTestCase assertExistenceOfBookmarkWithURL:@"http://www.a.fr/"
                                                       name:@"n5"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify old URL is back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Copy URL functionality on single URL selection.
- (void)testCopyFunctionalityOnSingleURL {
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Invoke Copy through context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Select Copy URL.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_COPY)]
      performAction:grey_tap()];

  // Wait so that the string is copied to clipboard.
  testing::WaitUntilConditionOrTimeout(1, ^{
    return false;
  });
  // Verify general pasteboard has the URL copied.
  NSString* copiedString = [UIPasteboard generalPasteboard].string;
  GREYAssert([copiedString containsString:@"www.a.fr"],
             @"The URL is not copied");
}

- (void)testContextMenuForMultipleURLSelection {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
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

// Verify the Open All functionality on multiple url selection.
- (void)testContextMenuForMultipleURLOpenAll {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Open 3 normal tabs from a normal session.
  [BookmarksNewGenTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                               IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 3 normal tabs.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 0,
                 @"Incognito tab count should be 0");

  // Verify the order of open tabs.
  [BookmarksNewGenTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  // Switch to Incognito mode by adding a new incognito tab.
  chrome_test_util::OpenNewIncognitoTab();

  [BookmarksNewGenTestCase openBookmarks];

  // Open 3 normal tabs from a incognito session.
  [BookmarksNewGenTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                               IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN];

  // Verify there are 6 normal tabs and no new incognito tabs.
  [ChromeEarlGrey waitForMainTabCount:6];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 1,
                 @"Incognito tab count should be 1");

  // Close the incognito tab to go back to normal mode.
  chrome_test_util::CloseAllIncognitoTabs();

  // The following verifies the selected bookmarks are open in the same order as
  // in folder.

  // Verify the order of open tabs.
  [BookmarksNewGenTestCase verifyOrderOfTabsWithCurrentTabIndex:3];
}

// Verify the Open All in Incognito functionality on multiple url selection.
- (void)testContextMenuForMultipleURLOpenAllInIncognito {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Open 3 incognito tabs from a normal session.
  [BookmarksNewGenTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                               IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // Verify there are 3 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:3];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 1,
                 @"Main tab count should be 1");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to switch to incognito mode");

  // Verify the order of open tabs.
  [BookmarksNewGenTestCase verifyOrderOfTabsWithCurrentTabIndex:0];

  [BookmarksNewGenTestCase openBookmarks];

  // Open 3 incognito tabs from a incognito session.
  [BookmarksNewGenTestCase selectUrlsAndTapOnContextBarButtonWithLabelId:
                               IDS_IOS_BOOKMARK_CONTEXT_MENU_OPEN_INCOGNITO];

  // The 3rd tab will be re-used to open one of the selected bookmarks.  So
  // there will be 2 new tabs only.

  // Verify there are 5 incognito tabs and no new normal tab.
  [ChromeEarlGrey waitForIncognitoTabCount:5];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 1,
                 @"Main tab count should be 1");

  // Verify the order of open tabs.
  [BookmarksNewGenTestCase verifyOrderOfTabsWithCurrentTabIndex:2];
}

// Verify the Open and Open in Incognito functionality on single url.
- (void)testOpenSingleBookmarkInNormalAndIncognitoTab {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Open a bookmark in current tab in a normal session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getFirstURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksNewGenTestCase openBookmarks];

  // Open a bookmark in new tab from a normal session (through a long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Verify there is 1 new normal tab created and no new incognito tab created.
  [ChromeEarlGrey waitForMainTabCount:2];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 0,
                 @"Incognito tab count should be 0");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getSecondURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksNewGenTestCase openBookmarks];

  // Open a bookmark in an incognito tab from a normal session (through a long
  // press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Verify there is 1 incognito tab created and no new normal tab created.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to switch to incognito mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getFrenchURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksNewGenTestCase openBookmarks];

  // Open a bookmark in current tab from a incognito session.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Verify "First URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getFirstURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to staying at incognito mode");

  // Verify no new tabs created.
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 1,
                 @"Incognito tab count should be 1");
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 2,
                 @"Main tab count should be 2");

  [BookmarksNewGenTestCase openBookmarks];

  // Open a bookmark in new incognito tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Verify a new incognito tab is created.
  [ChromeEarlGrey waitForIncognitoTabCount:2];
  GREYAssertTrue(chrome_test_util::GetMainTabCount() == 2,
                 @"Main tab count should be 2");

  // Verify the current tab is an incognito tab.
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Failed to staying at incognito mode");

  // Verify "Second URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getSecondURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  [BookmarksNewGenTestCase openBookmarks];

  // Open a bookmark in a new normal tab from a incognito session (through a
  // long press).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_longPress()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Verify a new normal tab is created and no incognito tab is created.
  [ChromeEarlGrey waitForMainTabCount:3];
  GREYAssertTrue(chrome_test_util::GetIncognitoTabCount() == 2,
                 @"Incognito tab count should be 2");

  // Verify the current tab is a normal tab.
  GREYAssertFalse(chrome_test_util::IsIncognitoMode(),
                  @"Failed to switch to normal mode");

  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getFrenchURL().GetContent())]
      assertWithMatcher:grey_notNil()];
}

- (void)testContextBarForSingleFolderSelection {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap Edit Folder.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify it shows edit view controller.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Editor")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testContextMenuForMultipleFolderSelection {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [self verifyContextMenuForMultiAndMixedSelection];
}

- (void)testContextMenuForMixedSelection {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [self verifyContextMenuForMultiAndMixedSelection];
}

- (void)testLongPressOnSingleURL {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  // Verify context menu.
  [self verifyContextMenuForSingleURL];
}

- (void)testLongPressOnSingleFolder {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the context menu.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];

  // Come back to the root.
  [[EarlGrey selectElementWithMatcher:BookmarksBackButton()]
      performAction:grey_tap()];

  // Long press on Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Mobile Bookmarks")]
      performAction:grey_longPress()];

  // Verify it doesn't show the context menu. (long press is disabled on
  // permanent node.)
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_nil()];
}

// Verify Edit functionality for single folder selection.
- (void)testEditFunctionalityOnSingleFolder {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Editor")]
      assertWithMatcher:grey_notNil()];
  NSString* existingFolderTitle = @"Folder 1";
  NSString* newFolderTitle = @"New Folder Title";
  [BookmarksNewGenTestCase renameBookmarkFolderWithFolderTitle:newFolderTitle];

  [BookmarksNewGenTestCase closeEditBookmarkFolder];

  // Verify that the change has been made.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(existingFolderTitle)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_notNil()];
}

// Verify Move functionality on single folder selection.
- (void)testMoveFunctionalityOnSingleFolder {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move the bookmark folder - "Folder 1" into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Create New Folder")]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksNewGenTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose new parent folder for "Title For New Folder" folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify Folder 2 only has one item.
  [BookmarksNewGenTestCase assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Select Folder 2 as new parent folder for "Title For New Folder".
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];

  // Verify folder picker is dismissed and folder creator is now visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Creator")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_notVisible()];

  // Verify picked parent folder is Folder 2.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Folder 2"), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done (accessibilityID is 'Save') to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Save")]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [self verifyFolderFlowIsClosed];

  // Verify new folder "Title For New Folder" has been created under Folder 2.
  [BookmarksNewGenTestCase assertChildCount:2 ofFolderWithName:@"Folder 2"];

  // Verify new folder "Title For New Folder" has one bookmark folder.
  [BookmarksNewGenTestCase assertChildCount:1
                           ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Folder 1.1" has been moved and assert it's presence.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on multiple folder selection.
- (void)testMoveFunctionalityOnMultipleFolder {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select multiple folders.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1.1")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into a new folder.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Create New Folder")]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksNewGenTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done (accessibilityID is 'Save') to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Save")]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [self verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksNewGenTestCase waitForUndoToastToGoAway];

  // Close edit mode.
  [BookmarksNewGenTestCase closeContextBarEditMode];

  // Verify new folder "Title For New Folder" has two bookmark folder.
  [BookmarksNewGenTestCase assertChildCount:2
                           ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Folder 1.1" and "Folder 1" have been moved and assert
  // it's presence.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on mixed folder / url selection.
- (void)testMoveFunctionalityOnMixedSelection {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Create New Folder")]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksNewGenTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder for "Title For New Folder" folder is "Mobile
  // Bookmarks" folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done (accessibilityID is 'Save') to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Save")]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [self verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksNewGenTestCase waitForUndoToastToGoAway];

  // Close edit mode.
  [BookmarksNewGenTestCase closeContextBarEditMode];

  // Verify new folder "Title For New Folder" has two bookmark nodes.
  [BookmarksNewGenTestCase assertChildCount:2
                           ofFolderWithName:@"Title For New Folder"];

  // Drill down to where "Second URL" and "Folder 1" have been moved and assert
  // it's presence.
  [[EarlGrey selectElementWithMatcher:TappableBookmarkNodeWithLabel(
                                          @"Title For New Folder")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verify Move functionality on multiple url selection.
- (void)testMoveFunctionalityOnMultipleUrlSelection {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode, using context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URL and folder.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"First URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on move, from context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Choose to move into Folder 1.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [self verifyFolderFlowIsClosed];

  // Wait for Undo toast to go away from screen.
  [BookmarksNewGenTestCase waitForUndoToastToGoAway];

  // Close edit mode.
  [BookmarksNewGenTestCase closeContextBarEditMode];

  // Verify Folder 1 has three bookmark nodes.
  [BookmarksNewGenTestCase assertChildCount:3 ofFolderWithName:@"Folder 1"];

  // Drill down to where "Second URL" and "First URL" have been moved and assert
  // it's presence.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Try deleting a bookmark from the edit screen, then undoing that delete.
- (void)testUndoDeleteBookmarkFromEditScreen {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap Edit Folder.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
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

  // Verify edit mode is closed (context bar back to default state).
  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];
}

- (void)testDeleteSingleFolderNode {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
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

  // Verify edit mode is closed (context bar back to default state).
  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];
}

- (void)testDeleteMultipleNodes {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
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

  // Verify edit mode is closed (context bar back to default state).
  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];
}

// Tests that the promo view is only seen at root level and not in any of the
// child nodes.
- (void)testPromoViewIsSeenOnlyInRootNode {
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

// Tests the creation of new folders by tapping on 'New Folder' button of the
// context bar.
- (void)testCreateNewFolderWithContextBar {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Create a new folder and name it "New Folder 1".
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarksNewGenTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                      pressReturn:YES];

  // Verify "New Folder 1" is created.
  [BookmarksNewGenTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and name it "New Folder 2".
  newFolderTitle = @"New Folder 2";
  [BookmarksNewGenTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                      pressReturn:YES];

  // Verify "New Folder 2" is created.
  [BookmarksNewGenTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Verify context bar does not change after editing folder name.
  [self verifyContextBarInDefaultStateWithSelectEnabled:YES];
}

// Tests the new folder name is committed when name editing is interrupted by
// navigating away.
- (void)testNewFolderNameCommittedOnNavigatingAway {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Create a new folder and type "New Folder 1" without pressing return.
  NSString* newFolderTitle = @"New Folder 1";
  [BookmarksNewGenTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                      pressReturn:NO];

  // Interrupt the folder name editing by tapping on back.
  [[EarlGrey selectElementWithMatcher:BookmarksBackButton()]
      performAction:grey_tap()];
  // Come back to Mobile Bookmarks.
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Verify folder name "New Folder 1" was committed.
  [BookmarksNewGenTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 2" without pressing return.
  newFolderTitle = @"New Folder 2";
  [BookmarksNewGenTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                      pressReturn:NO];

  // Interrupt the folder name editing by tapping on done.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
  // Reopen bookmarks.
  [BookmarksNewGenTestCase openBookmarks];

  // Verify folder name "New Folder 2" was committed.
  [BookmarksNewGenTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 3" without pressing return.
  newFolderTitle = @"New Folder 3";
  [BookmarksNewGenTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                      pressReturn:NO];

  // Interrupt the folder name editing by entering Folder 1
  [BookmarksNewGenTestCase scrollToTop];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:BookmarksBackButton()]
      performAction:grey_tap()];

  // Verify folder name "New Folder 3" was committed.
  [BookmarksNewGenTestCase verifyFolderCreatedWithTitle:newFolderTitle];

  // Create a new folder and type "New Folder 4" without pressing return.
  newFolderTitle = @"New Folder 4";
  [BookmarksNewGenTestCase createNewBookmarkFolderWithFolderTitle:newFolderTitle
                                                      pressReturn:NO];

  // Interrupt the folder name editing by tapping on First URL.
  [BookmarksNewGenTestCase scrollToTop];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  // Reopen bookmarks.
  [BookmarksNewGenTestCase openBookmarks];

  // Verify folder name "New Folder 4" was committed.
  [BookmarksNewGenTestCase verifyFolderCreatedWithTitle:newFolderTitle];
}

- (void)testEmptyBackgroundAndSelectButton {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Enter Folder 1.1 (which is empty)
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      performAction:grey_tap()];

  // Verify the empty background appears.
  [self verifyEmptyBackgroundAppears];

  // Come back to Mobile Bookmarks.
  [[EarlGrey selectElementWithMatcher:BookmarksBackButton()]
      performAction:grey_tap()];

  // Change to edit mode, using context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select every URL and folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1.1")]
      performAction:grey_tap()];

  // Tap delete on context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait for Undo toast to go away from screen.
  [BookmarksNewGenTestCase waitForUndoToastToGoAway];

  // Verify edit mode is close automatically (context bar switched back to
  // default state) and select button is disabled.
  [self verifyContextBarInDefaultStateWithSelectEnabled:NO];

  // Verify the empty background appears.
  [self verifyEmptyBackgroundAppears];
}

- (void)testCachePositionIsRecreated {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];

  // Reopen bookmarks.
  [BookmarksNewGenTestCase openBookmarks];

  // Ensure the contents of Folder 1 is visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testCachePositionIsRecreatedWhenNodeIsDeleted {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Select Folder 2.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];

  // Delete Folder 2.
  [BookmarksNewGenTestCase removeBookmarkWithTitle:@"Folder 2"];

  // Reopen bookmarks.
  [BookmarksNewGenTestCase openBookmarks];

  // Ensure the root node is opened, by verifying folders at this level.
  [BookmarksNewGenTestCase verifyBookmarkFolderIsSeen:@"Mobile Bookmarks"];
}

- (void)testCachePositionIsRecreatedWhenNodeIsMoved {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Select Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];

  // Select Folder 2.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Select Folder 3
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 3")]
      performAction:grey_tap()];

  // Close bookmarks
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];

  // Move Folder 3 under Folder 1.
  [BookmarksNewGenTestCase moveBookmarkWithTitle:@"Folder 3"
                               toFolderWithTitle:@"Folder 1"];

  // Reopen bookmarks.
  [BookmarksNewGenTestCase openBookmarks];

  // Go back 1 level.
  [[EarlGrey selectElementWithMatcher:BookmarksBackButton()]
      performAction:grey_tap()];

  // Ensure Folder 1 is seen, by verifying folders at this level.
  [BookmarksNewGenTestCase verifyBookmarkFolderIsSeen:@"Folder 2"];
}

// Tests that all elements on the bookmarks landing page are accessible.
- (void)testAccessibilityOnBookmarksLandingPage {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on mobile bookmarks are accessible.
- (void)testAccessibilityOnMobileBookmarks {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks folder Edit page are accessible.
- (void)testAccessibilityOnBookmarksFolderEditPage {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Editor")]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Edit page are accessible.
- (void)testAccessibilityOnBookmarksEditPage {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Invoke Edit through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Move page are accessible.
- (void)testAccessibilityOnBookmarksMovePage {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Move to New Folder page are
// accessible.
- (void)testAccessibilityOnBookmarksMoveToNewFolderPage {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Invoke Move through long press.
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Folder 1")]
      performAction:grey_longPress()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      performAction:grey_tap()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Create New Folder")]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on bookmarks Delete and Undo are accessible.
- (void)testAccessibilityOnBookmarksDeleteUndo {
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
  [[EarlGrey
      selectElementWithMatcher:TappableBookmarkNodeWithLabel(@"Second URL")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarDeleteString])]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksNewGenTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that all elements on the bookmarks Select page are accessible.
- (void)testAccessibilityOnBookmarksSelect {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kBookmarkNewGeneration);

  [BookmarksNewGenTestCase setupStandardBookmarks];
  [BookmarksNewGenTestCase openBookmarks];
  [BookmarksNewGenTestCase openMobileBookmarks];

  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
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

  NSString* firstTitle = @"First URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(firstTitle), getFirstURL());

  NSString* secondTitle = @"Second URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(secondTitle), getSecondURL());

  NSString* frenchTitle = @"French URL";
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(frenchTitle), getFrenchURL());

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

// Wait for Undo toast to go away.
+ (void)waitForUndoToastToGoAway {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for undo toast to go away");
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

+ (void)assertExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  const bookmarks::BookmarkNode* bookmark =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          GURL(base::SysNSStringToUTF16(URL)));
  GREYAssert(bookmark->GetTitle() == base::SysNSStringToUTF16(name),
             @"Could not find bookmark named %@ for %@", name, URL);
}

// Rename folder title to |folderTitle|. Must be in edit folder UI.
+ (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  NSString* titleIdentifier = @"Title_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(folderTitle)];
}

// Dismisses the edit folder UI.
+ (void)closeEditBookmarkFolder {
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
}

// Close edit mode.
+ (void)closeContextBarEditMode {
  [[EarlGrey selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarCancelString])]
      performAction:grey_tap()];
}

// Select urls from Mobile Bookmarks and tap on a specified context bar button.
+ (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId {
  // Change to edit mode
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_trailing_button")]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                          [BookmarksNewGenTestCase
                                              contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Open All.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(buttonLabelId)]
      performAction:grey_tap()];
}

// Verify the Mobile Bookmarks's urls are open in the same order as they are in
// folder.
+ (void)verifyOrderOfTabsWithCurrentTabIndex:(NSUInteger)tabIndex {
  // Verify "French URL" appears in the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getFrenchURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "Second URL" appears.
  // TODO(crbug.com/695749): see we if can add switchToNextTab to
  // chrome_test_util so that we don't need to pass tabIndex here.
  chrome_test_util::SelectTabAtIndexInCurrentMode(tabIndex + 1);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getSecondURL().GetContent())]
      assertWithMatcher:grey_notNil()];

  // Switch to the next Tab and verify "First URL" appears.
  chrome_test_util::SelectTabAtIndexInCurrentMode(tabIndex + 2);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          getFirstURL().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Verifies that there is |count| children on the bookmark folder with |name|.
+ (void)assertChildCount:(int)count ofFolderWithName:(NSString*)name {
  base::string16 name16(base::SysNSStringToUTF16(name));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());

  const bookmarks::BookmarkNode* folder = NULL;
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_folder() && bookmark->GetTitle() == name16) {
      folder = bookmark;
      break;
    }
  }
  GREYAssert(folder, @"No folder named %@", name);
  GREYAssertEqual(
      folder->child_count(), count,
      @"Unexpected number of children in folder '%@': %d instead of %d", name,
      folder->child_count(), count);
}

- (void)verifyContextMenuForSingleURL {
  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_CONTENT_CONTEXT_COPY)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)verifyContextMenuForMultiAndMixedSelection {
  // Verify it shows the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_context_menu")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"context_bar")]
      assertWithMatcher:grey_notNil()];

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
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   selectEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)verifyFolderFlowIsClosed {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Creator")]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Editor")]
      assertWithMatcher:grey_notVisible()];
}

- (void)verifyEmptyBackgroundAppears {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"empty_background_label")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Removes programmatically the first bookmark with the given title.
+ (void)removeBookmarkWithTitle:(NSString*)title {
  base::string16 name16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->GetTitle() == name16) {
      bookmarkModel->Remove(bookmark);
      return;
    }
  }
  GREYFail(@"Could not remove bookmark with name %@", title);
}

+ (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder {
  base::string16 name16(base::SysNSStringToUTF16(bookmarkTitle));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* bookmark = iterator.Next();
  while (iterator.has_next()) {
    if (bookmark->GetTitle() == name16) {
      break;
    }
    bookmark = iterator.Next();
  }

  base::string16 folderName16(base::SysNSStringToUTF16(newFolder));
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iteratorFolder(
      bookmarkModel->root_node());
  const bookmarks::BookmarkNode* folder = iteratorFolder.Next();
  while (iteratorFolder.has_next()) {
    if (folder->GetTitle() == folderName16) {
      break;
    }
    folder = iteratorFolder.Next();
  }
  std::set<const bookmarks::BookmarkNode*> toMove;
  toMove.insert(bookmark);
  bookmark_utils_ios::MoveBookmarks(toMove, bookmarkModel, folder);
}

+ (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(
                                       NSClassFromString(@"UITableViewCell")),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Scroll the bookmarks to top.
+ (void)scrollToTop {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
}

// Scroll the bookmarks to bottom.
+ (void)scrollToBottom {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmarksTableView")]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
}

// Verify a folder with given name is created and it is not being edited.
+ (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle {
  // scroll to bottom to make sure new folder appears.
  [BookmarksNewGenTestCase scrollToBottom];
  // verify the folder is created.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(folderTitle)]
      assertWithMatcher:grey_notNil()];
  // verify the editable textfield is gone.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"bookmark_editing_text")]
      assertWithMatcher:grey_nil()];
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

+ (NSString*)contextBarMoreString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
}

// Create a new folder with given title.
+ (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn {
  // Click on "New Folder".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"context_bar_leading_button")]
      performAction:grey_tap()];

  NSString* titleIdentifier = @"bookmark_editing_text";

  // Type the folder title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(titleIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_replaceText(folderTitle)];

  // Press the keyboard return key.
  if (pressReturn) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityID(titleIdentifier),
                                     grey_sufficientlyVisible(), nil)]
        performAction:grey_typeText(@"\n")];

    // Wait until the editing textfield is gone.
    [BookmarksNewGenTestCase
        waitForDeletionOfBookmarkWithTitle:titleIdentifier];
  }
}

// TODO(crbug.com/695749): Add egtests for:
// 1. Spinner background.
// 2. Reorder bookmarks. (make sure it won't clear the row selection on table)
// 3. Current root node removed: Verify that the New Folder, Select button are
//    disabled and empty background appears when _currentRootNode becomes NULL
//    (maybe programmatically remove the current root node from model, and
//    trigger a sync).
// 4. Restoring y position when opening from cache.
// 5. Adding new folder when when existing bookmarks list covers full screen
//    height,to ensure we scroll to the newly added folder.
// 6. Test new folder name is committed when name editing is interrupted by
// tapping context bar buttons.

@end
