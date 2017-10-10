// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
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
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;

namespace {
// TODO(crbug.com/616929): Move common matchers that are useful across tests
// into a shared location.

// Matcher for bookmarks tool tip star.
id<GREYMatcher> StarButton() {
  return ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
}

// Matcher for the button to add bookmark.
id<GREYMatcher> AddBookmarkButton() {
  return ButtonWithAccessibilityLabelId(IDS_BOOKMARK_ADD_EDITOR_TITLE);
}

// Matcher for the lit star buttom on iPhone that will open the edit button
// screen.
id<GREYMatcher> LitStarButtoniPhone() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK);
}

// Matcher for the button to edit bookmark.
id<GREYMatcher> EditBookmarkButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_EDIT);
}

// Matcher for the button to close the tools menu.
id<GREYMatcher> CloseToolsMenuButton() {
  NSString* closeMenuButtonText =
      l10n_util::GetNSString(IDS_IOS_TOOLBAR_CLOSE_MENU);
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_accessibilityLabel(closeMenuButtonText), nil);
}

// Matcher for the Done button on the bookmarks UI.
id<GREYMatcher> BookmarksDoneButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_DONE_BUTTON),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitKeyboardKey)), nil);
}

// Matcher for the More Menu.
id<GREYMatcher> MoreMenuButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_BOOKMARK_NEW_MORE_BUTTON_ACCESSIBILITY_LABEL);
}

// Types of actions possible in the contextual action sheet.
typedef NS_ENUM(NSUInteger, Action) {
  ActionSelect,
  ActionEdit,
  ActionMove,
  ActionDelete,
};

// Matcher for the action sheet's buttons.
id<GREYMatcher> ActionSheet(Action action) {
  int accessibilityLabelMessageID;
  switch (action) {
    case ActionSelect:
      accessibilityLabelMessageID = IDS_IOS_BOOKMARK_ACTION_SELECT;
      break;
    case ActionEdit:
      accessibilityLabelMessageID = IDS_IOS_BOOKMARK_ACTION_EDIT;
      break;
    case ActionMove:
      accessibilityLabelMessageID = IDS_IOS_BOOKMARK_ACTION_MOVE;
      break;
    case ActionDelete:
      accessibilityLabelMessageID = IDS_IOS_BOOKMARK_ACTION_DELETE;
      break;
  }

  return grey_allOf(grey_accessibilityLabel(
                        l10n_util::GetNSString(accessibilityLabelMessageID)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_not(grey_accessibilityID(@"Edit_editing_bar")), nil);
}

}  // namespace

// Bookmark integration tests for Chrome.
@interface BookmarksTestCase : ChromeTestCase
@end

@implementation BookmarksTestCase

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

#pragma mark Tests

// Verifies that adding a bookmark and removing a bookmark via the UI properly
// updates the BookmarkModel.
- (void)testAddRemoveBookmark {
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();
  NSString* bookmarkTitle = @"my bookmark";

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  // Add the bookmark from the UI.
  [BookmarksTestCase bookmarkCurrentTabWithTitle:bookmarkTitle];

  // Verify the bookmark is set.
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkTitle expectedCount:1];

  NSString* const kStarLitLabel =
      !IsCompact() ? l10n_util::GetNSString(IDS_TOOLTIP_STAR)
                   : l10n_util::GetNSString(IDS_IOS_BOOKMARK_EDIT_SCREEN_TITLE);
  // Verify the star is lit.
  if (IsCompact()) {
    [ChromeEarlGreyUI openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kStarLitLabel)]
      assertWithMatcher:grey_notNil()];

  // Clear the bookmark via the UI.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kStarLitLabel)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionDelete)]
      performAction:grey_tap()];

  // Verify the bookmark is not in the BookmarkModel.
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkTitle expectedCount:0];

  NSString* const kStarUnlitLabel =
      !IsCompact() ? l10n_util::GetNSString(IDS_TOOLTIP_STAR)
                   : l10n_util::GetNSString(IDS_BOOKMARK_ADD_EDITOR_TITLE);

  // Verify the star is not lit.
  if (IsCompact()) {
    [ChromeEarlGreyUI openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kStarUnlitLabel)]
      assertWithMatcher:grey_notNil()];

  // TODO(crbug.com/617652): This code should be removed when a common helper
  // is added to close any menus, which should be run as test setup.
  if (IsCompact()) {
    [[EarlGrey selectElementWithMatcher:CloseToolsMenuButton()]
        performAction:grey_tap()];
  }

  // Close the opened tab.
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() closeCurrentTab];
}

// Tests that tapping a bookmark on the NTP navigates to the proper URL.
- (void)testTapBookmark {
  const GURL bookmarkURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  NSString* bookmarkTitle = @"smokeTapBookmark";

  // Load a bookmark into the bookmark model.
  [BookmarksTestCase addBookmark:bookmarkURL withTitle:bookmarkTitle];

  // Open the UI for Bookmarks.
  [BookmarksTestCase openMobileBookmarks];

  // Verify bookmark is visible.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(bookmarkTitle)]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:nil];

  // Tap on the bookmark and verify the URL that appears in the omnibox.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(bookmarkTitle)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          bookmarkURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Test to set bookmarks in multiple tabs.
- (void)testBookmarkMultipleTabs {
  const GURL firstURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  const GURL secondURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:firstURL];
  chrome_test_util::OpenNewTab();
  [ChromeEarlGrey loadURL:secondURL];

  [BookmarksTestCase bookmarkCurrentTabWithTitle:@"my bookmark"];
  [BookmarksTestCase assertBookmarksWithTitle:@"my bookmark" expectedCount:1];
}

// Try navigating to the bookmark screen, and selecting a bookmark.
- (void)testSelectBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Tap on one of the standard bookmark. Verify that it loads.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Second URL")]
      performAction:grey_tap()];

  // Wait for the page to load.
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Check the URL is correct.
  const GURL secondURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(secondURL.GetContent())];
}

// Try deleting a bookmark, then undoing that delete.
- (void)testUndoDeleteBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL Info")]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionDelete)]
      performAction:grey_tap()];

  // Wait until it's gone.
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Second URL"];

  // Press undo
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
}

// Try deleting a bookmark from the edit screen, then undoing that delete.
- (void)testUndoDeleteBookmarkFromEditScreen {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL Info")]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];

  // Delete it.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionDelete)]
      performAction:grey_tap()];

  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];
}

// Try moving bookmarks, then undoing that move.
- (void)testUndoMoveBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Verify that folder 2 only has 1 child.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL Info")]
      performAction:grey_tap()];

  // Select a first bookmark.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionSelect)]
      performAction:grey_tap()];

  // Select a second bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];

  // Choose the move action.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Move")]
      performAction:grey_tap()];

  // Pick the destination.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Wait for undo to show up (there is a 300ms delay for the user to see the
  // change).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that folder 2 has 3 children now, and that they are no longer
  // visible.
  [BookmarksTestCase assertChildCount:3 ofFolderWithName:@"Folder 2"];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notVisible()];

  // Press undo.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
      performAction:grey_tap()];

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Verify it's back.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      assertWithMatcher:grey_notNil()];

  // Verify that folder 2 is back to one child.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Folder 2"];
}

- (void)testLabelUpdatedUponMove {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Tap on the Edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder with default name.
  [BookmarksTestCase addFolderWithName:nil];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notNil()];

  // Check the new folder label.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"New Folder"), nil)]
      assertWithMatcher:grey_notNil()];
}

// Test the creation of a bookmark and new folder.
- (void)testAddBookmarkInNewFolder {
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  std::string expectedURLContent = bookmarkedURL.GetContent();

  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase starCurrentTab];

  // Verify the snackbar title.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Bookmarked")]
      assertWithMatcher:grey_notNil()];

  // Tap on the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Verify that the newly-created bookmark is in the BookmarkModel.
  [BookmarksTestCase
      assertBookmarksWithTitle:base::SysUTF8ToNSString(expectedURLContent)
                 expectedCount:1];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase assertFolderName:@"Mobile Bookmarks"];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder with default name.
  [BookmarksTestCase addFolderWithName:nil];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notNil()];

  [BookmarksTestCase assertFolderExists:@"New Folder"];
}

// Tests that changing a folder's title in edit mode works as expected.
- (void)testChangeFolderTitle {
  NSString* existingFolderTitle = @"Folder 1";
  NSString* newFolderTitle = @"New Folder Title";

  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];
  [BookmarksTestCase openEditBookmarkFolderWithFolderTitle:existingFolderTitle];
  [BookmarksTestCase renameBookmarkFolderWithFolderTitle:newFolderTitle];
  [BookmarksTestCase closeEditBookmarkFolder];

  if (IsCompact()) {
    // Exit from bookmarks modal. IPad shows bookmarks in tab.
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
  }

  // Verify that the change has been made.
  [BookmarksTestCase openMobileBookmarks];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(existingFolderTitle)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newFolderTitle)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the default folder bookmarks are saved in is updated to the last
// used folder.
- (void)testStickyDefaultFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Tap on the top-right button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder.
  [BookmarksTestCase addFolderWithName:@"Sticky Folder"];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notVisible()];

  if (IsCompact()) {
    // Dismiss the bookmarks screen.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Exit")]
        performAction:grey_tap()];
  }

  // Second, bookmark a page.

  // Verify that the bookmark that is going to be added is not in the
  // BookmarkModel.
  const GURL bookmarkedURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/fullscreen.html");
  NSString* const bookmarkedURLString =
      base::SysUTF8ToNSString(bookmarkedURL.spec());
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkedURLString
                                expectedCount:0];
  // Open the page.
  std::string expectedURLContent = bookmarkedURL.GetContent();
  [ChromeEarlGrey loadURL:bookmarkedURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          expectedURLContent)]
      assertWithMatcher:grey_notNil()];

  // Verify that the folder has only one element.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Sticky Folder"];

  // Bookmark the page.
  [BookmarksTestCase starCurrentTab];

  // Verify the snackbar title.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"Bookmarked to Sticky Folder")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the newly-created bookmark is in the BookmarkModel.
  [BookmarksTestCase assertBookmarksWithTitle:bookmarkedURLString
                                expectedCount:1];

  // Verify that the folder has now two elements.
  [BookmarksTestCase assertChildCount:2 ofFolderWithName:@"Sticky Folder"];
}

// Tests that changes to the parent folder from the Single Bookmark Controller
// are saved to the bookmark only when saving the results.
- (void)testMoveDoesSaveOnSave {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];

  // Tap on the top-right button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];

  // Tap the Folder button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Create a new folder.
  [BookmarksTestCase addFolderWithName:nil];

  // Verify that the editor is present.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the new folder doesn't contain the bookmark.
  [BookmarksTestCase assertChildCount:0 ofFolderWithName:@"New Folder"];

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notVisible()];

  // Check that the new folder contains the bookmark.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"New Folder"];

  // Dismiss the bookmarks screen.
  if (IsCompact()) {
    // Dismiss the bookmarks screen.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Exit")]
        performAction:grey_tap()];
  }

  // Check that the new folder still contains the bookmark.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"New Folder"];
}

// Test thats editing a single bookmark correctly persists data.
- (void)testSingleBookmarkEdit {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];
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
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"n5")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [BookmarksTestCase assertExistenceOfBookmarkWithURL:@"http://www.a.fr"
                                                 name:@"n5"];
}

// Tests that cancelling editing a single bookmark correctly doesn't persist
// data.
- (void)testSingleBookmarkCancelEdit {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Title Field_textField")]
      performAction:grey_replaceText(@"n5")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"URL Field_textField")]
      performAction:grey_replaceText(@"www.a.fr")];

  // Dismiss editor with Cancel button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Cancel")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Single Bookmark Editor")]
      assertWithMatcher:grey_notVisible()];

  // Verify that the bookmark was not updated.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"n5")]
      assertWithMatcher:grey_notVisible()];
  [BookmarksTestCase assertAbsenceOfBookmarkWithURL:@"http://www.a.fr"];
}

// Tests that long pressing a bookmark selects it and gives access to editing,
// as does the Info menu.
- (void)testLongPressBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];

  // Long press the top-right button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_longPress()];

  // Tap the edit button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Edit_editing_bar")]
      performAction:grey_tap()];

  // Dismiss the editor screen.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];

  // Tap on the top-right button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];

  // Dismiss the editor screen.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
}

// Tests the editing of a folder.
- (void)testEditFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarkFolder:@"Folder 1"];

  // Tap the Edit button in the navigation bar.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Edit_navigation_bar")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Title_textField")]
      performAction:grey_replaceText(@"Renamed Folder")];

  // Cancel without saving.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Cancel")]
      performAction:grey_tap()];

  // Check that Folder 1 still exists at this name, and Renamed Folder doesn't.
  [BookmarksTestCase assertFolderExistsWithTitle:@"Folder 1"];
  [BookmarksTestCase assertFolderDoesntExistWithTitle:@"Renamed Folder"];

  // Tap the Edit button in the navigation bar.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Edit_navigation_bar")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Title_textField")]
      performAction:grey_replaceText(@"Renamed Folder")];

  // Save.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Save")]
      performAction:grey_tap()];

  // Check that Folder 1 doesn't exist and Renamed Folder does.
  [BookmarksTestCase assertFolderDoesntExistWithTitle:@"Folder 1"];
  [BookmarksTestCase assertFolderExistsWithTitle:@"Renamed Folder"];
}

// Tests the deletion of a folder.
- (void)testDeleteFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarkFolder:@"Folder 1"];

  // Delete the folder.
  [BookmarksTestCase deleteSelectedFolder];

  // Check that the folder doesn't exist anymore.
  [BookmarksTestCase assertFolderDoesntExistWithTitle:@"Folder 1"];
}

// Navigates to a deeply nested folder, deletes it and makes sure the UI is
// consistent.
- (void)testDeleteCurrentSubfolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarkFolder:@"Folder 1"];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Folder 3")]
      performAction:grey_tap()];

  // Delete the folder.
  [BookmarksTestCase deleteSelectedFolder];

  // Folder 3 is now deleted, UI should have moved to Folder 2, and Folder 2
  // should be empty.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 2")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [BookmarksTestCase assertChildCount:0 ofFolderWithName:@"Folder 2"];
  [BookmarksTestCase assertFolderDoesntExistWithTitle:@"Folder 3"];
  [BookmarksTestCase waitForDeletionOfBookmarkWithTitle:@"Folder 3"];
}

// Navigates to a deeply nested folder, delete its parent programatically.
// Verifies that the UI is as expected.
- (void)testDeleteParentFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarkFolder:@"Folder 1"];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Folder 3")]
      performAction:grey_tap()];

  // Remove the parent programmatically.
  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 2"];

  // Folder 2 and 3 are now deleted, UI should have moved to Folder1, and
  // Folder 1 should be empty.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(NSClassFromString(
                                       @"BookmarkNavigationBar")),
                                   grey_descendant(grey_text(@"Folder 1")),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [BookmarksTestCase assertChildCount:0 ofFolderWithName:@"Folder 1"];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 2")]
      assertWithMatcher:grey_notVisible()];
  [BookmarksTestCase assertFolderDoesntExistWithTitle:@"Folder 2"];
  [BookmarksTestCase assertFolderDoesntExistWithTitle:@"Folder 3"];

  // Check that the selected folder in the menu is Folder 1.
  if (IsCompact()) {
    // Opens the bookmark manager sidebar on handsets.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        performAction:grey_tap()];
  }
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(
                                       NSClassFromString(@"BookmarkMenuCell")),
                                   grey_descendant(grey_text(@"Folder 1")),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the menu button changes to a back button as expected when browsing
// nested folders.
- (void)testBrowseNestedFolders {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Navigate down the nested folders.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 1")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 3")]
      performAction:grey_tap()];

  // Verify the back button is visible to be able to go back to parent.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Parent")]
      assertWithMatcher:grey_sufficientlyVisible()];

  if (IsCompact()) {
    // Verify menu button becomes back button in phone only.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        assertWithMatcher:grey_notVisible()];
  }

  // Go back two levels to Folder 1.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Parent")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Parent")]
      performAction:grey_tap()];

  // Verify back button is hidden again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Parent")]
      assertWithMatcher:grey_notVisible()];

  if (IsCompact()) {
    // Verify menu button reappears.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Tests moving a bookmark into a new folder created in the moving process.
- (void)testCreateNewFolderWhileMovingBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Tap the info disclosure indicator for the bookmark we want to move.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL Info")]
      performAction:grey_tap()];

  // Choose to move the bookmark in the context menu.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionMove)]
      performAction:grey_tap()];

  // Choose to move the bookmark into a new folder.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Create New Folder")]
      performAction:grey_tap()];

  // Enter custom new folder name.
  [BookmarksTestCase
      renameBookmarkFolderWithFolderTitle:@"Title For New Folder"];

  // Verify current parent folder (Change Folder) is Bookmarks folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Mobile Bookmarks"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Choose new parent folder (Change Folder).
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify Folder 2 only has one item.
  [BookmarksTestCase assertChildCount:1 ofFolderWithName:@"Folder 2"];

  // Select Folder 2 as new Change Folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder 2")]
      performAction:grey_tap()];

  // Verify folder picker is dismissed and folder creator is now visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Creator")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_notVisible()];

  // Verify picked parent folder (Change Folder) is Folder 2.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(@"Folder 2"), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done (accessibilityID is 'Save') to close bookmark move flow.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Save")]
      performAction:grey_tap()];

  // Verify all folder flow UI is now closed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Creator")]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Editor")]
      assertWithMatcher:grey_notVisible()];

  // Verify new folder has been created under Folder 2.
  [BookmarksTestCase assertChildCount:2 ofFolderWithName:@"Folder 2"];

  // Verify new folder has one bookmark.
  [BookmarksTestCase assertChildCount:1
                     ofFolderWithName:@"Title For New Folder"];
}

// Navigates to a deeply nested folder, deletes its root ancestor and checks
// that the UI is on the top level folder.
- (void)testDeleteRootFolder {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openBookmarkFolder:@"Folder 1"];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Folder 2")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(@"Folder 3")]
      performAction:grey_tap()];

  [BookmarksTestCase removeBookmarkWithTitle:@"Folder 1"];

  NSString* rootFolderTitle = nil;
  rootFolderTitle = @"Mobile Bookmarks";

  // Folder 2 and 3 are now deleted, UI should have moved to top level folder.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(NSClassFromString(
                                       @"BookmarkNavigationBar")),
                                   grey_descendant(grey_text(rootFolderTitle)),
                                   nil)] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      assertWithMatcher:grey_notVisible()];

  if (IsCompact()) {
    // Opens the bookmark manager sidebar on handsets.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        performAction:grey_tap()];

    // Test that the root folder is selected in the menu. This is only the case
    // on iPhone.
    GREYElementMatcherBlock* selectedMatcher =
        [GREYElementMatcherBlock matcherWithMatchesBlock:^BOOL(id element) {
          UITableViewCell* cell = (UITableViewCell*)element;
          return [cell isSelected];
        }
            descriptionBlock:^void(id<GREYDescription> description) {
              [description appendText:@"Selected UI element."];
            }];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_kindOfClass(NSClassFromString(
                                                @"BookmarkMenuCell")),
                                            grey_descendant(
                                                grey_text(rootFolderTitle)),
                                            nil)]
        assertWithMatcher:selectedMatcher];
  }

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Folder 1")]
      assertWithMatcher:grey_notVisible()];
}

// Tests that keyboard commands are registered when a bookmark is added with the
// new bookmark UI as it shows only a snackbar.
- (void)testKeyboardCommandsRegistered_AddBookmark {
  // Add the bookmark.
  [BookmarksTestCase starCurrentTab];
  GREYAssertTrue(chrome_test_util::GetRegisteredKeyCommandsCount() > 0,
                 @"Some keyboard commands are registered.");
}

// Tests that keyboard commands are not registered when a bookmark is edited, as
// the edit screen is presented modally.
- (void)testKeyboardCommandsNotRegistered_EditBookmark {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openMobileBookmarks];

  // Go to a bookmarked page. Tap on one of the standard bookmark.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Second URL")]
      performAction:grey_tap()];

  // Edit the bookmark.
  if (!IsCompact()) {
    [[EarlGrey selectElementWithMatcher:StarButton()] performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [[EarlGrey selectElementWithMatcher:LitStarButtoniPhone()]
        performAction:grey_tap()];
  }
  GREYAssertTrue(chrome_test_util::GetRegisteredKeyCommandsCount() == 0,
                 @"No keyboard commands are registered.");
}

// Tests that tapping No thanks on the promo make it disappear.
- (void)testPromoNoThanksMakeItDisappear {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];

  // We are going to set the PromoAlreadySeen preference. Set a teardown handler
  // to reset it.
  [self setTearDownHandler:^{
    [BookmarksTestCase setPromoAlreadySeen:NO];
  }];
  // Check that sign-in promo view is visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];

  // Tap the dismiss button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSigninPromoCloseButtonId)]
      performAction:grey_tap()];

  // Wait until promo is gone.
  [SigninEarlGreyUtils checkSigninPromoNotVisible];

  // Check that the promo already seen state is updated.
  [BookmarksTestCase verifyPromoAlreadySeen:YES];
}

// Tests the tapping on the primary button of sign-in promo view in a cold
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithColdStateUsingPrimaryButton {
  [BookmarksTestCase openBookmarks];

  // Check that sign-in promo view are visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
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
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
}

// Tests the tapping on the primary button of sign-in promo view in a warm
// state makes the confirmaiton sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingPrimaryButton {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];

  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Check that sign-in promo view are visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];

  // Tap the secondary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Tap the UNDO button.
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"UNDO")]
      performAction:grey_tap()];

  // Check that the bookmarks UI reappeared and the cell is still here.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
}

// Tests the tapping on the secondary button of sign-in promo view in a warm
// state makes the sign-in sheet appear, and the promo still appears after
// dismissing the sheet.
- (void)testSignInPromoWithWarmStateUsingSecondaryButton {
  [BookmarksTestCase setupStandardBookmarks];
  [BookmarksTestCase openTopLevelBookmarksFolder];
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Check that sign-in promo view are visible.
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
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
  [BookmarksTestCase verifyPromoAlreadySeen:NO];
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeWarmState];
}

// Tests that the sign-in promo should not be shown after been shown 19 times.
- (void)testAutomaticSigninPromoDismiss {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, 19);
  [BookmarksTestCase openBookmarks];
  // Check the sign-in promo view is visible.
  [SigninEarlGreyUtils
      checkSigninPromoVisibleWithMode:SigninPromoViewModeColdState];
  // Check the sign-in promo will not be shown anymore.
  [BookmarksTestCase verifyPromoAlreadySeen:YES];
  GREYAssertEqual(
      20, prefs->GetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount),
      @"Should have incremented the display count");
  // Close the bookmark view and open it again.
  if (IsCompact()) {
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
    [BookmarksTestCase openBookmarks];
  } else {
    [BookmarksTestCase closeAllTabs];
    chrome_test_util::OpenNewTab();
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Check that the sign-in promo is not visible anymore.
  [SigninEarlGreyUtils checkSigninPromoNotVisible];
}

// Tests that all elements on the bookmarks landing page are accessible.
- (void)testAccessibilityOnBookmarksLandingPage {
  [BookmarksTestCase openMobileBookmarksPrepopulatedWithOneBookmark];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  if (IsCompact()) {
    // Exit from bookmarks modal. IPad shows bookmarks in tab.
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
  }
}

// Tests that all elements on the bookmarks Edit page are accessible.
- (void)testAccessibilityOnBookmarksEditPage {
  [BookmarksTestCase openMobileBookmarksPrepopulatedWithOneBookmark];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:MoreMenuButton()]
      performAction:grey_tap()];

  // Tap the edit action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionEdit)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  if (IsCompact()) {
    // Exit from bookmarks modal. IPad shows bookmarks in tab.
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
  }
}

// Tests that all elements on the bookmarks Move page are accessible.
- (void)testAccessibilityOnBookmarksMovePage {
  [BookmarksTestCase openMobileBookmarksPrepopulatedWithOneBookmark];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:MoreMenuButton()]
      performAction:grey_tap()];

  // Tap the Move action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionMove)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  if (IsCompact()) {
    // Exit from bookmarks modal. IPad shows bookmarks in tab.
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
  }
}

// Tests that all elements on the bookmarks Move to New Folder page are
// accessible.
- (void)testAccessibilityOnBookmarksMoveToNewFolderPage {
  [BookmarksTestCase openMobileBookmarksPrepopulatedWithOneBookmark];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:MoreMenuButton()]
      performAction:grey_tap()];

  // Tap the Move action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionMove)]
      performAction:grey_tap()];
  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Create New Folder")]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  if (IsCompact()) {
    // Exit from bookmarks modal. IPad shows bookmarks in tab.
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
  }
}

// Tests that all elements on bookmarks Delete and Undo are accessible.
- (void)testAccessibilityOnBookmarksDeleteUndo {
  [BookmarksTestCase openMobileBookmarksPrepopulatedWithOneBookmark];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:MoreMenuButton()]
      performAction:grey_tap()];

  // Tap the Delete action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionDelete)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  if (IsCompact()) {
    // Exit from bookmarks modal. IPad shows bookmarks in tab.
    [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
        performAction:grey_tap()];
  }
}

// Tests that all elements on the bookmarks Select page are accessible.
- (void)testAccessibilityOnBookmarksSelect {
  [BookmarksTestCase openMobileBookmarksPrepopulatedWithOneBookmark];

  // Load the menu for a bookmark.
  [[EarlGrey selectElementWithMatcher:MoreMenuButton()]
      performAction:grey_tap()];

  // Tap the Select action.
  [[EarlGrey selectElementWithMatcher:ActionSheet(ActionSelect)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  // Dismiss selector with Cancel button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Exit Edit Mode")]
      performAction:grey_tap()];
}

#pragma mark Helper Methods

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

// Navigates to the bookmark manager UI, and selects |bookmarkFolder|.
+ (void)openBookmarkFolder:(NSString*)bookmarkFolder {
  [BookmarksTestCase openBookmarks];
  if (IsCompact()) {
    // Opens the bookmark manager sidebar on handsets.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Menu")]
        performAction:grey_tap()];
  }

  // Selects the folder with label |bookmarkFolder|.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClass(
                                       NSClassFromString(@"BookmarkMenuCell")),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)] performAction:grey_tap()];
}

// Navigates to the bookmark manager UI, and selects the top level folder.
+ (void)openTopLevelBookmarksFolder {
  [BookmarksTestCase openMobileBookmarks];
}

// Navigates to the bookmark manager UI, and selects MobileBookmarks.
+ (void)openMobileBookmarks {
  [BookmarksTestCase openBookmarkFolder:@"Mobile Bookmarks"];
}

// Adds a bookmark, then navigates to the bookmark manager UI, and
// selects MobileBookmarks.
+ (void)openMobileBookmarksPrepopulatedWithOneBookmark {
  const GURL bookmarkURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  NSString* bookmarkTitle = @"smokeTapBookmark";
  // Load a bookmark into the bookmark model.
  [BookmarksTestCase addBookmark:bookmarkURL withTitle:bookmarkTitle];

  // Open the UI for Bookmarks.
  [BookmarksTestCase openMobileBookmarks];

  // Verify bookmark is visible.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(bookmarkTitle)]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:nil];
}

// Navigates to the edit folder UI for |folderTitle|.
+ (void)openEditBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(folderTitle)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:EditBookmarkButton()]
      performAction:grey_tap()];
}

// Dismisses the edit folder UI.
+ (void)closeEditBookmarkFolder {
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
}

// Rename folder title to |folderTitle|. Must be in edit folder UI.
+ (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  NSString* titleIdentifier = @"Title_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(folderTitle)];
}

// Tap on the star to bookmark a page, then edit the bookmark to change the
// title to |title|.
+ (void)bookmarkCurrentTabWithTitle:(NSString*)title {
  [BookmarksTestCase waitForBookmarkModelLoaded:YES];
  // Add the bookmark from the UI.
  [BookmarksTestCase starCurrentTab];

  // Set the bookmark name.
  [[EarlGrey selectElementWithMatcher:EditBookmarkButton()]
      performAction:grey_tap()];
  NSString* titleIdentifier = @"Title Field_textField";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(titleIdentifier)]
      performAction:grey_replaceText(title)];

  // Dismiss the window.
  [[EarlGrey selectElementWithMatcher:BookmarksDoneButton()]
      performAction:grey_tap()];
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

// Asserts that a folder called |title| exists.
+ (void)assertFolderExists:(NSString*)title {
  base::string16 folderTitle16(base::SysNSStringToUTF16(title));
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  BOOL folderExists = NO;

  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url())
      continue;
    // This is a folder.
    if (bookmark->GetTitle() == folderTitle16) {
      folderExists = YES;
      break;
    }
  }

  NSString* assertMessage =
      [NSString stringWithFormat:@"Folder %@ doesn't exist", title];
  GREYAssert(folderExists, assertMessage);
}

// Asserts that |expectedCount| bookmarks exist with the corresponding |title|
// using the BookmarkModel.
+ (void)assertBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount {
  // Get BookmarkModel and wait for it to be loaded.
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  // Verify the correct number of bookmarks exist.
  base::string16 matchString = base::SysNSStringToUTF16(title);
  std::vector<bookmarks::TitledUrlMatch> matches;
  bookmarkModel->GetBookmarksMatching(matchString, 50, &matches);
  const size_t count = matches.size();
  GREYAssertEqual(expectedCount, count, @"Unexpected number of bookmarks");
}

// Check that the currently edited bookmark is in |folderName| folder.
+ (void)assertFolderName:(NSString*)folderName {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(folderName), nil)]
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

// Adds a bookmark with the given |url| and |title| into the Mobile Bookmarks
// folder.
+ (void)addBookmark:(const GURL)url withTitle:(NSString*)title {
  [BookmarksTestCase waitForBookmarkModelLoaded:YES];
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(title), url);
}

// Creates a new folder starting from the folder picker.
// Passing a |name| of 0 length will use the default value.
+ (void)addFolderWithName:(NSString*)name {
  // Wait for folder picker to appear.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Picker")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Create New Folder")]
      performAction:grey_tap()];

  // Verify the folder creator is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Folder Creator")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Change the name of the folder.
  if (name.length > 0) {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(@"Title_textField")]
        performAction:grey_replaceText(name)];
  }

  // Tap the Save button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Save")]
      performAction:grey_tap()];
}

// Loads a set of default bookmarks in the model for the tests to use.
+ (void)setupStandardBookmarks {
  [BookmarksTestCase waitForBookmarkModelLoaded:YES];

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

+ (void)assertAbsenceOfBookmarkWithURL:(NSString*)URL {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  const bookmarks::BookmarkNode* bookmark =
      bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          GURL(base::SysNSStringToUTF16(URL)));
  GREYAssert(!bookmark, @"There is a bookmark for %@", URL);
}

// Whether there is a bookmark folder with the given title.
+ (BOOL)folderExistsWithTitle:(NSString*)folderTitle {
  base::string16 folderTitle16(base::SysNSStringToUTF16(folderTitle));
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarkModel->root_node());

  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* bookmark = iterator.Next();
    if (bookmark->is_url())
      continue;
    // This is a folder.
    if (bookmark->GetTitle() == folderTitle16)
      return YES;
  }
  return NO;
}

// Asserts that there is a bookmark folder with the given title.
+ (void)assertFolderExistsWithTitle:(NSString*)folderTitle {
  GREYAssert([BookmarksTestCase folderExistsWithTitle:folderTitle],
             @"There is no folder named %@", folderTitle);
}

// Asserts that there is no bookmark folder with the given title.
+ (void)assertFolderDoesntExistWithTitle:(NSString*)folderTitle {
  GREYAssert(![BookmarksTestCase folderExistsWithTitle:folderTitle],
             @"There is a folder named %@", folderTitle);
}

// Deletes via the UI the currently focused folder. This must be called once
// already in a non permanent folder (i.e. not Mobile Bookmarks, etc.).
+ (void)deleteSelectedFolder {
  // Enter edit mode.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"Edit_navigation_bar")]
      performAction:grey_tap()];

  // Delete the folder.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Delete Folder")]
      performAction:grey_tap()];
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

// Adds a bookmark for the current tab. Must be called when on a tab.
+ (void)starCurrentTab {
  if (!IsCompact()) {
    [[EarlGrey selectElementWithMatcher:StarButton()] performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openToolsMenu];
    [[EarlGrey selectElementWithMatcher:AddBookmarkButton()]
        performAction:grey_tap()];
  }
}

@end
