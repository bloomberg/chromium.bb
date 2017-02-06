// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kReadTitle[] = "foobar";
const char kReadURL[] = "http://readfoobar.com";
const char kUnreadTitle[] = "I am an unread entry";
const char kUnreadURL[] = "http://unreadfoobar.com";
const char kReadURL2[] = "http://kReadURL2.com";
const char kReadTitle2[] = "read item 2";
const char kUnreadTitle2[] = "I am another unread entry";
const char kUnreadURL2[] = "http://unreadfoobar2.com";
const size_t kNumberReadEntries = 2;
const size_t kNumberUnreadEntries = 2;
const CFTimeInterval kSnackbarAppearanceTimeout = 5;
const CFTimeInterval kSnackbarDisappearanceTimeout =
    MDCSnackbarMessageDurationMax + 1;
const char kReadHeader[] = "Read";
const char kUnreadHeader[] = "Unread";

// Returns the reading list model.
ReadingListModel* GetReadingListModel() {
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  GREYAssert(testing::WaitUntilConditionOrTimeout(2,
                                                  ^{
                                                    return model->loaded();
                                                  }),
             @"Reading List model did not load");
  return model;
}

// Asserts the |button_id| button is not visible.
void AssertButtonNotVisibleWithID(int button_id) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   button_id)]
      assertWithMatcher:grey_notVisible()];
}

// Assert the |button_id| button is visible.
void AssertButtonVisibleWithID(int button_id) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   button_id)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Taps the |button_id| button.
void TapButtonWithID(int button_id) {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   button_id)] performAction:grey_tap()];
}

// Taps the entry |title|.
void TapEntry(std::string title) {
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                                base::SysUTF8ToNSString(title)),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

// Asserts that the entry |title| is visible.
void AssertEntryVisible(std::string title) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(title)),
                     grey_ancestor(grey_kindOfClass([ReadingListCell class])),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Asserts that all the entries are visible.
void AssertAllEntriesVisible() {
  AssertEntryVisible(kReadTitle);
  AssertEntryVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);

  // If the number of entries changes, make sure this assert gets updated.
  GREYAssertEqual((size_t)2, kNumberReadEntries,
                  @"The number of entries have changed");
  GREYAssertEqual((size_t)2, kNumberUnreadEntries,
                  @"The number of entries have changed");
}

// Asserts that the entry |title| is not visible.
void AssertEntryNotVisible(std::string title) {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(title)),
                     grey_ancestor(grey_kindOfClass([ReadingListCell class])),
                     nil)] assertWithMatcher:grey_notVisible()];
}

// Asserts |header| is visible.
void AssertHeaderNotVisible(std::string header) {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabel(
                                              base::SysUTF8ToNSString(header))]
      assertWithMatcher:grey_notVisible()];
}

// Opens the reading list menu using command line.
void OpenReadingList() {
  GenericChromeCommand* command =
      [[GenericChromeCommand alloc] initWithTag:IDC_SHOW_READING_LIST];
  chrome_test_util::RunCommandWithActiveViewController(command);
}

// Adds a read and an unread entry to the model, opens the reading list menu and
// enter edit mode.
void AddEntriesAndEnterEdit() {
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(GURL(kReadURL), std::string(kReadTitle),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->SetReadStatus(GURL(kReadURL), true);
  model->AddEntry(GURL(kReadURL2), std::string(kReadTitle2),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->SetReadStatus(GURL(kReadURL2), true);
  model->AddEntry(GURL(kUnreadURL), std::string(kUnreadTitle),
                  reading_list::ADDED_VIA_CURRENT_APP);
  model->AddEntry(GURL(kUnreadURL2), std::string(kUnreadTitle2),
                  reading_list::ADDED_VIA_CURRENT_APP);
  OpenReadingList();

  TapButtonWithID(IDS_IOS_READING_LIST_EDIT_BUTTON);
}

// Computes the number of read entries in the model.
size_t ModelReadSize(ReadingListModel* model) {
  size_t size = 0;
  for (const auto& url : model->Keys()) {
    if (model->GetEntryByURL(url)->IsRead()) {
      size++;
    }
  }
  return size;
}
}  // namespace

// Test class for the Reading List menu.
@interface ReadingListTestCase : ChromeTestCase

@end

@implementation ReadingListTestCase

- (void)setUp {
  [super setUp];
  ReadingListModel* model = GetReadingListModel();
  for (const GURL& url : model->Keys())
    model->RemoveEntryByURL(url);
}

// Tests that the Reading List view is accessible.
- (void)testAccessibility {
  AddEntriesAndEnterEdit();
  // In edit mode.
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  TapButtonWithID(IDS_IOS_READING_LIST_CANCEL_BUTTON);
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
}

// Tests that sharing a web page to the Reading List results in a snackbar
// appearing, and that the Reading List entry is present in the Reading List.
- (void)testSavingToReadingList {
  // Setup a server serving a page at http://potato with the title "tomato".
  std::map<GURL, std::string> responses;
  const GURL regularPageURL = web::test::HttpServer::MakeUrl("http://potato");
  responses[regularPageURL] = "<html><head><title>tomato</title></head></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Open http://potato
  [ChromeEarlGrey loadURL:regularPageURL];

  // Add the page to the reading list.
  [ChromeEarlGreyUI openShareMenu];
  TapButtonWithID(IDS_IOS_SHARE_MENU_READING_LIST_ACTION);

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbarMatcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_READING_LIST_SNACKBAR_MESSAGE);
  ConditionBlock waitForAppearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbarMatcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  // Wait for the snackbar to disappear.
  GREYAssert(testing::WaitUntilConditionOrTimeout(kSnackbarAppearanceTimeout,
                                                  waitForAppearance),
             @"Snackbar did not appear.");
  ConditionBlock waitForDisappearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbarMatcher]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(kSnackbarDisappearanceTimeout,
                                                  waitForDisappearance),
             @"Snackbar did not disappear.");

  // Verify that a page with the title "tomato" is present in the reading list.
  OpenReadingList();
  AssertEntryVisible("tomato");
}

// Tests that only the "Edit" button is showing when not editing.
- (void)testVisibleButtonsNonEditingMode {
  GetReadingListModel()->AddEntry(GURL(kUnreadURL), std::string(kUnreadTitle),
                                  reading_list::ADDED_VIA_CURRENT_APP);
  OpenReadingList();

  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_DELETE_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_CANCEL_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_EDIT_BUTTON);
}

// Tests that only the "Cancel", "Delete All Read" and "Mark All…" buttons are
// showing when not editing.
- (void)testVisibleButtonsEditingModeEmptySelection {
  AddEntriesAndEnterEdit();

  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_DELETE_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_EDIT_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_CANCEL_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark Unread" buttons are showing
// when not editing.
- (void)testVisibleButtonsOnlyReadEntrySelected {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_EDIT_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_DELETE_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_CANCEL_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark Read" buttons are showing
// when not editing.
- (void)testVisibleButtonsOnlyUnreadEntrySelected {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_EDIT_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_DELETE_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_CANCEL_BUTTON);
}

// Tests that only the "Cancel", "Delete" and "Mark…" buttons are showing when
// not editing.
- (void)testVisibleButtonsMixedEntriesSelected {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_EDIT_BUTTON);
  AssertButtonNotVisibleWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_MARK_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_DELETE_BUTTON);
  AssertButtonVisibleWithID(IDS_IOS_READING_LIST_CANCEL_BUTTON);
}

// Tests the deletion of selected entries.
- (void)testDeleteEntries {
  AddEntriesAndEnterEdit();

  TapEntry(kReadTitle2);

  TapButtonWithID(IDS_IOS_READING_LIST_DELETE_BUTTON);

  AssertEntryVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries, GetReadingListModel()->unread_size());
}

// Tests the deletion of all read entries.
- (void)testDeleteAllReadEntries {
  AddEntriesAndEnterEdit();

  TapButtonWithID(IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON);

  AssertEntryNotVisible(kReadTitle);
  AssertEntryNotVisible(kReadTitle2);
  AssertHeaderNotVisible(kReadHeader);
  AssertEntryVisible(kUnreadTitle);
  AssertEntryVisible(kUnreadTitle2);
  XCTAssertEqual((size_t)0, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries, GetReadingListModel()->unread_size());
}

// Marks all unread entries as read.
- (void)testMarkAllRead {
  AddEntriesAndEnterEdit();

  TapButtonWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);

  // Tap the action sheet.
  TapButtonWithID(IDS_IOS_READING_LIST_MARK_ALL_READ_ACTION);

  AssertHeaderNotVisible(kUnreadHeader);
  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberUnreadEntries + kNumberReadEntries,
                 ModelReadSize(GetReadingListModel()));
  XCTAssertEqual((size_t)0, GetReadingListModel()->unread_size());
}

// Marks all read entries as unread.
- (void)testMarkAllUnread {
  AddEntriesAndEnterEdit();

  TapButtonWithID(IDS_IOS_READING_LIST_MARK_ALL_BUTTON);

  // Tap the action sheet.
  TapButtonWithID(IDS_IOS_READING_LIST_MARK_ALL_UNREAD_ACTION);

  AssertHeaderNotVisible(kReadHeader);
  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberUnreadEntries + kNumberReadEntries,
                 GetReadingListModel()->unread_size());
  XCTAssertEqual((size_t)0, ModelReadSize(GetReadingListModel()));
}

// Selects an unread entry and mark it as read.
- (void)testMarkEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kUnreadTitle);

  TapButtonWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries + 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries - 1,
                 GetReadingListModel()->unread_size());
}

// Selects an read entry and mark it as unread.
- (void)testMarkEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);

  TapButtonWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries + 1,
                 GetReadingListModel()->unread_size());
}

// Selects read and unread entries and mark them as unread.
- (void)testMarkMixedEntriesUnread {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  TapButtonWithID(IDS_IOS_READING_LIST_MARK_BUTTON);

  // Tap the action sheet.
  TapButtonWithID(IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries - 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries + 1,
                 GetReadingListModel()->unread_size());
}

// Selects read and unread entries and mark them as read.
- (void)testMarkMixedEntriesRead {
  AddEntriesAndEnterEdit();
  TapEntry(kReadTitle);
  TapEntry(kUnreadTitle);

  TapButtonWithID(IDS_IOS_READING_LIST_MARK_BUTTON);

  // Tap the action sheet.
  TapButtonWithID(IDS_IOS_READING_LIST_MARK_READ_BUTTON);

  AssertAllEntriesVisible();
  XCTAssertEqual(kNumberReadEntries + 1, ModelReadSize(GetReadingListModel()));
  XCTAssertEqual(kNumberUnreadEntries - 1,
                 GetReadingListModel()->unread_size());
}

@end
