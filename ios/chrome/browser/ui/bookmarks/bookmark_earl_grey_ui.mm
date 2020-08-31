// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"

#include "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Redefine EarlGrey macro to use line number and file name taken from the place
// of BookmarkEarlGreyUIImpl macro instantiation, rather than local line number
// inside test helper method. Original EarlGrey macro definition also expands to
// EarlGreyImpl instantiation. [self earlGrey] is provided by a superclass and
// returns EarlGreyImpl object created with correct line number and filename.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define EarlGrey [self earlGrey]
#pragma clang diagnostic pop

using chrome_test_util::BookmarksMenuButton;
using chrome_test_util::BookmarksSaveEditDoneButton;
using chrome_test_util::BookmarksSaveEditFolderButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextBarCenterButtonWithLabel;
using chrome_test_util::ContextBarLeadingButtonWithLabel;
using chrome_test_util::ContextBarTrailingButtonWithLabel;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::TappableBookmarkNodeWithLabel;

namespace chrome_test_util {

id<GREYMatcher> StarButton() {
  return ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
}

id<GREYMatcher> BookmarksDeleteSwipeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BOOKMARK_ACTION_DELETE);
}

id<GREYMatcher> BookmarkHomeDoneButton() {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> BookmarksSaveEditDoneButton() {
  return grey_accessibilityID(kBookmarkEditNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> BookmarksSaveEditFolderButton() {
  return grey_accessibilityID(
      kBookmarkFolderEditNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> ContextBarLeadingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeLeadingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContextBarCenterButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeCenterButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContextBarTrailingButtonWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(kBookmarkHomeTrailingButtonIdentifier),
                    grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TappableBookmarkNodeWithLabel(NSString* label) {
  return grey_allOf(grey_accessibilityID(label), grey_sufficientlyVisible(),
                    nil);
}

id<GREYMatcher> SearchIconButton() {
  return grey_accessibilityID(kBookmarkHomeSearchBarIdentifier);
}

}  // namespace chrome_test_util

@implementation BookmarkEarlGreyUIImpl

- (void)openBookmarks {
  // Opens the bookmark manager.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:BookmarksMenuButton()];

  // Assert the menu is gone.
  [[EarlGrey selectElementWithMatcher:BookmarksMenuButton()]
      assertWithMatcher:grey_nil()];
}

- (void)openMobileBookmarks {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UITableViewCell"),
                                   grey_descendant(
                                       grey_text(@"Mobile Bookmarks")),
                                   nil)] performAction:grey_tap()];
}

- (void)starCurrentTab {
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuAddToBookmarks),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];
}

- (void)addFolderWithName:(NSString*)name {
  // Wait for folder picker to appear.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on "Create New Folder."
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkCreateNewFolderCellIdentifier)]
      performAction:grey_tap()];

  // Verify the folder creator is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Change the name of the folder.
  if (name.length > 0) {
    [[EarlGrey
        selectElementWithMatcher:[self
                                     textFieldMatcherForID:@"Title_textField"]]
        performAction:grey_replaceText(name)];
  }

  // Tap the Done button.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditFolderButton()]
      performAction:grey_tap()];
}

- (void)waitForDeletionOfBookmarkWithTitle:(NSString*)title {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(title)]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
             @"Waiting for bookmark to go away");
}

- (void)waitForUndoToastToGoAway {
  // Wait until it's gone.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Undo")]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  EG_TEST_HELPER_ASSERT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(10, condition),
      @"Waiting for undo toast to go away");
}

- (void)renameBookmarkFolderWithFolderTitle:(NSString*)folderTitle {
  NSString* titleIdentifier = @"Title_textField";
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_replaceText(folderTitle)];
}

- (void)closeContextBarEditMode {
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarCancelString])]
      performAction:grey_tap()];
}

- (void)selectUrlsAndTapOnContextBarButtonWithLabelId:(int)buttonLabelId {
  // Change to edit mode
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkHomeTrailingButtonIdentifier)]
      performAction:grey_tap()];

  // Select URLs.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"First URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Second URL")]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"French URL")]
      performAction:grey_tap()];

  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  // Tap on Open All.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(buttonLabelId)]
      performAction:grey_tap()];
}

- (void)verifyContextMenuForSingleURLWithEditEnabled:(BOOL)editEnabled {
  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  // Verify that the edit menu option is enabled/disabled according to
  // |editEnabled|.
  id<GREYMatcher> matcher =
      editEnabled ? grey_sufficientlyVisible()
                  : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)]
      assertWithMatcher:matcher];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)verifyContextMenuForSingleFolderWithEditEnabled:(BOOL)editEnabled {
  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify options on context menu.
  // Verify that the edit menu option is enabled/disabled according to
  // |editEnabled|.
  id<GREYMatcher> matcher =
      editEnabled ? grey_sufficientlyVisible()
                  : grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT_FOLDER)]
      assertWithMatcher:matcher];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)]
      assertWithMatcher:matcher];
}

- (void)dismissContextMenu {
  // Verify it shows the context menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeContextMenuIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the context menu. On non compact width tap the Bookmarks TableView
  // to dismiss, since there might not be a cancel button.
  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kBookmarkHomeTableViewIdentifier)]
        performAction:grey_tap()];
  }
}

- (void)verifyContextBarInDefaultStateWithSelectEnabled:(BOOL)selectEnabled
                                       newFolderEnabled:(BOOL)newFolderEnabled {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Verify context bar shows enabled "New Folder" and enabled "Select".
  [[EarlGrey selectElementWithMatcher:ContextBarLeadingButtonWithLabel(
                                          [BookmarkEarlGreyUI
                                              contextBarNewFolderString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   newFolderEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:ContextBarTrailingButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarSelectString])]
      assertWithMatcher:grey_allOf(grey_notNil(),
                                   selectEnabled
                                       ? grey_enabled()
                                       : grey_accessibilityTrait(
                                             UIAccessibilityTraitNotEnabled),
                                   nil)];
}

- (void)verifyContextBarInEditMode {
  // Verify the context bar is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeUIToolbarIdentifier)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      assertWithMatcher:grey_notNil()];
}

- (void)verifyFolderFlowIsClosed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderCreateViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBookmarkFolderEditViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

- (void)verifyEmptyBackgroundAppears {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkEmptyStateExplanatoryLabelIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)verifyBookmarkFolderIsSeen:(NSString*)bookmarkFolder {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UITableViewCell"),
                                   grey_descendant(grey_text(bookmarkFolder)),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)scrollToBottom {
  // Provide a start points since it prevents some tests timing out under
  // certain configurations.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeTableViewIdentifier)]
      performAction:grey_scrollToContentEdgeWithStartPoint(
                        kGREYContentEdgeBottom, 0.5, 0.5)];
}

- (void)verifyFolderCreatedWithTitle:(NSString*)folderTitle {
  // scroll to bottom to make sure new folder appears.
  [BookmarkEarlGreyUI scrollToBottom];
  // verify the folder is created.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(folderTitle)]
      assertWithMatcher:grey_notNil()];
  // verify the editable textfield is gone.
  [[EarlGrey selectElementWithMatcher:
                 [self textFieldMatcherForID:@"bookmark_editing_text"]]
      assertWithMatcher:grey_notVisible()];
}

- (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
             setParentFolderTo:(NSString*)destinationFolder
                          from:(NSString*)sourceFolder {
  // Tap context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the edit page (editor) is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Verify current parent folder for is correct.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(sourceFolder), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on Folder to open folder picker.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Change Folder")]
      performAction:grey_tap()];

  // Verify folder picker UI is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select the new destination folder. Use grey_ancestor since
  // BookmarksHomeTableView might be visible on the background on non-compact
  // widthts, and there might be a "destinationFolder" node there as well.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(TappableBookmarkNodeWithLabel(destinationFolder),
                            grey_ancestor(grey_accessibilityID(
                                kBookmarkFolderPickerViewContainerIdentifier)),
                            nil)] performAction:grey_tap()];

  // Verify folder picker is dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kBookmarkFolderPickerViewContainerIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify parent folder has been changed in edit page.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"Change Folder"),
                                   grey_accessibilityLabel(destinationFolder),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss edit page (editor).
  id<GREYMatcher> dismissMatcher = BookmarksSaveEditDoneButton();
  // If a folder is being edited use the EditFolder button dismiss matcher
  // instead.
  if ([editorId isEqualToString:kBookmarkFolderEditViewContainerIdentifier])
    dismissMatcher = BookmarksSaveEditFolderButton();
  [[EarlGrey selectElementWithMatcher:dismissMatcher] performAction:grey_tap()];

  // Verify the Editor was dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];

  // Wait for Undo toast to go away from screen.
  [BookmarkEarlGreyUI waitForUndoToastToGoAway];
}

- (void)tapOnLongPressContextMenuButton:(int)menuButtonId
                                 onItem:(id<GREYMatcher>)item
                             openEditor:(NSString*)editorId
                        modifyTextField:(NSString*)textFieldId
                                     to:(NSString*)newName
                            dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through item context menu.
  [[EarlGrey selectElementWithMatcher:item] performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:[self textFieldMatcherForID:textFieldId]]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

- (void)tapOnContextMenuButton:(int)menuButtonId
                    openEditor:(NSString*)editorId
               modifyTextField:(NSString*)textFieldId
                            to:(NSString*)newName
                   dismissWith:(NSString*)dismissButtonId {
  // Invoke Edit through context menu.
  [[EarlGrey
      selectElementWithMatcher:ContextBarCenterButtonWithLabel(
                                   [BookmarkEarlGreyUI contextBarMoreString])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(menuButtonId)]
      performAction:grey_tap()];

  // Verify that the editor is present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notNil()];

  // Edit textfield.
  [[EarlGrey selectElementWithMatcher:[self textFieldMatcherForID:textFieldId]]
      performAction:grey_replaceText(newName)];

  // Dismiss editor.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(dismissButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(editorId)]
      assertWithMatcher:grey_notVisible()];
}

- (NSString*)contextBarNewFolderString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_NEW_FOLDER);
}

- (NSString*)contextBarDeleteString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_DELETE);
}

- (NSString*)contextBarCancelString {
  return l10n_util::GetNSString(IDS_CANCEL);
}

- (NSString*)contextBarSelectString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_EDIT);
}

- (NSString*)contextBarMoreString {
  return l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_BAR_MORE);
}

- (void)createNewBookmarkFolderWithFolderTitle:(NSString*)folderTitle
                                   pressReturn:(BOOL)pressReturn {
  // Click on "New Folder".
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBookmarkHomeLeadingButtonIdentifier)]
      performAction:grey_tap()];

  NSString* titleIdentifier = @"bookmark_editing_text";

  // Type the folder title.
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_replaceText(folderTitle)];

  // Press the keyboard return key.
  if (pressReturn) {
    [[EarlGrey
        selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
        performAction:grey_typeText(@"\n")];

    // Wait until the editing textfield is gone.
    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey
          selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
          assertWithMatcher:grey_notVisible()
                      error:&error];
      return error == nil;
    };
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(10, condition),
               @"Waiting for textfield to go away");
  }
}

- (void)bookmarkCurrentTabWithTitle:(NSString*)title {
  [BookmarkEarlGreyUI starCurrentTab];

  // Set the bookmark name.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_BOOKMARK_ACTION_EDIT)]
      performAction:grey_tap()];

  NSString* titleIdentifier = @"Title Field_textField";
  [[EarlGrey
      selectElementWithMatcher:[self textFieldMatcherForID:titleIdentifier]]
      performAction:grey_replaceText(title)];

  // Dismiss the window.
  [[EarlGrey selectElementWithMatcher:BookmarksSaveEditDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Helpers

- (id<GREYMatcher>)textFieldMatcherForID:(NSString*)accessibilityID {
  return grey_allOf(grey_accessibilityID(accessibilityID),
                    grey_kindOfClassName(@"UITextField"),
                    grey_sufficientlyVisible(), nil);
}

@end
