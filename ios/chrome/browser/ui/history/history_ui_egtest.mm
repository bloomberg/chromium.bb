// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

using chrome_test_util::buttonWithAccessibilityLabelId;

namespace {
char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kResponse1[] = "Test Page 1";
char kResponse2[] = "Test Page 2";
char kResponse3[] = "Test Page 3";

// Matcher for entry in history for URL.
id<GREYMatcher> historyEntryWithURL(const GURL& url) {
  NSString* url_spec = base::SysUTF8ToNSString(url.spec());
  return grey_allOf(grey_text(url_spec), grey_sufficientlyVisible(), nil);
}
// Matcher for the history button in the tools menu.
id<GREYMatcher> historyButton() {
  return buttonWithAccessibilityLabelId(IDS_HISTORY_SHOW_HISTORY);
}
// Matcher for the done button in the navigation bar.
id<GREYMatcher> navigationDoneButton() {
  // Include sufficientlyVisible condition for the case of the clear browsing
  // dialog, which also has a "Done" button and is displayed over the history
  // panel.
  return grey_allOf(
      buttonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON),
      grey_sufficientlyVisible(), nil);
}
// Matcher for the edit button in the navigation bar.
id<GREYMatcher> navigationEditButton() {
  return buttonWithAccessibilityLabelId(IDS_HISTORY_START_EDITING_BUTTON);
}
// Matcher for the delete button.
id<GREYMatcher> deleteHistoryEntriesButton() {
  // Include class restriction to exclude MDCCollectionViewInfoBar, which is
  // hidden.
  return grey_allOf(buttonWithAccessibilityLabelId(
                        IDS_HISTORY_DELETE_SELECTED_ENTRIES_BUTTON),
                    grey_kindOfClass([UIButton class]), nil);
}
// Matcher for the search button.
id<GREYMatcher> searchIconButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_ICON_SEARCH);
}
// Matcher for the cancel button.
id<GREYMatcher> cancelButton() {
  return buttonWithAccessibilityLabelId(IDS_HISTORY_CANCEL_EDITING_BUTTON);
}
// Matcher for the button to open the clear browsing data panel.
id<GREYMatcher> openClearBrowsingDataButton() {
  return buttonWithAccessibilityLabelId(
      IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
}
// Matcher for the Open in New Tab option in the context menu.
id<GREYMatcher> openInNewTabButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
}
// Matcher for the Open in New Incognito Tab option in the context menu.
id<GREYMatcher> openInNewIncognitoTabButton() {
  return buttonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
}
// Matcher for the Copy URL option in the context menu.
id<GREYMatcher> copyUrlButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_COPY);
}
// Matcher for the clear cookies cell on the clear browsing data panel.
id<GREYMatcher> clearCookiesButton() {
  return grey_accessibilityID(kClearCookiesCellId);
}
// Matcher for the clear cache cell on the clear browsing data panel.
id<GREYMatcher> clearCacheButton() {
  return grey_allOf(grey_accessibilityID(kClearCacheCellId),
                    grey_sufficientlyVisible(), nil);
}
// Matcher for the clear browsing data button on the clear browsing data panel.
id<GREYMatcher> clearBrowsingDataButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_CLEAR_BUTTON);
}

}  // namespace

// History UI tests.
@interface HistoryUITestCase : ChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}

// Loads three test URLs.
- (void)loadTestURLs;
// Displays the history UI.
- (void)openHistoryPanel;
// Asserts that the history UI displays no history entries.
- (void)assertNoHistoryShown;
// Resets which data is selected in the Clear Browsing Data UI.
- (void)resetBrowsingDataPrefs;

@end

@implementation HistoryUITestCase

// Set up called once for the class.
+ (void)setUp {
  [super setUp];
  std::map<GURL, std::string> responses;
  responses[web::test::HttpServer::MakeUrl(kURL1)] = kResponse1;
  responses[web::test::HttpServer::MakeUrl(kURL2)] = kResponse2;
  responses[web::test::HttpServer::MakeUrl(kURL3)] = kResponse3;
  web::test::SetUpSimpleHttpServer(responses);
}

- (void)setUp {
  [super setUp];
  _URL1 = web::test::HttpServer::MakeUrl(kURL1);
  _URL2 = web::test::HttpServer::MakeUrl(kURL2);
  _URL3 = web::test::HttpServer::MakeUrl(kURL3);
  [ChromeEarlGrey clearBrowsingHistory];
  // Some tests rely on a clean state for the "Clear Browsing Data" settings
  // screen.
  [self resetBrowsingDataPrefs];
}

- (void)tearDown {
  NSError* error = nil;
  // Dismiss search bar by pressing cancel, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:cancelButton()] performAction:grey_tap()
                                                              error:&error];
  // Dismiss history panel by pressing done, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:navigationDoneButton()]
      performAction:grey_tap()
              error:&error];

  // Some tests change the default values for the "Clear Browsing Data" settings
  // screen.
  [self resetBrowsingDataPrefs];
  [super tearDown];
}

#pragma mark Tests

// Tests that no history is shown if there has been no navigation.
- (void)testDisplayNoHistory {
  [self openHistoryPanel];
  [self assertNoHistoryShown];
}

// Tests that the history panel displays navigation history.
- (void)testDisplayHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that history displays three entries.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL3)]
      assertWithMatcher:grey_notNil()];

  // Tap a history entry and assert that navigation to that entry's URL occurs.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      performAction:grey_tap()];
  id<GREYMatcher> webViewMatcher =
      chrome_test_util::webViewContainingText(kResponse1);
  [[EarlGrey selectElementWithMatcher:webViewMatcher]
      assertWithMatcher:grey_notNil()];
}

// Tests that searching history displays only entries matching the search term.
- (void)testSearchHistory {
  [self loadTestURLs];
  [self openHistoryPanel];
  [[EarlGrey selectElementWithMatcher:searchIconButton()]
      performAction:grey_tap()];

  NSString* searchString =
      [NSString stringWithFormat:@"%s", _URL1.path().c_str()];
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_typeText(searchString)];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL2)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL3)]
      assertWithMatcher:grey_nil()];
}

// Tests deletion of history entries.
- (void)testDeleteHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that three history elements are present.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL3)]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select a history element, and press delete.
  [[EarlGrey selectElementWithMatcher:navigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:deleteHistoryEntriesButton()]
      performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL3)]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select both remaining entries, and press delete.
  [[EarlGrey selectElementWithMatcher:navigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL3)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:deleteHistoryEntriesButton()]
      performAction:grey_tap()];

  [self assertNoHistoryShown];
}

// Tests clear browsing history.
- (void)testClearBrowsingHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Open the Clear Browsing Data dialog.
  [[EarlGrey selectElementWithMatcher:openClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Uncheck "Cookies, Site Data" and "Cached Images and Files," which are
  // checked by default, and press "Clear Browsing Data"
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearBrowsingDataButton()]
      performAction:grey_tap()];

  // There is not currently a matcher for acessibilityElementIsFocused or
  // userInteractionEnabled which could be used here instead of checking that
  // the button is not a MDCCollectionViewTextCell. Use when available.
  // TODO(crbug.com/638674): Evaluate if this can move to shared code.
  id<GREYMatcher> confirmClear = grey_allOf(
      clearBrowsingDataButton(),
      grey_not(grey_kindOfClass([MDCCollectionViewTextCell class])), nil);
  [[EarlGrey selectElementWithMatcher:confirmClear] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:navigationDoneButton()]
      performAction:grey_tap()];

  [self assertNoHistoryShown];
}

// Tests display and selection of 'Open in New Tab' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewTab {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      performAction:grey_longPress()];

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [[EarlGrey selectElementWithMatcher:openInNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::omniboxText(
                                          _URL1.GetContent())]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::AssertMainTabCount(2);
}

// Tests display and selection of 'Open in New Incognito Tab' in a context menu
// on a history entry.
- (void)testContextMenuOpenInNewIncognitoTab {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      performAction:grey_longPress()];

  // Select "Open in New Incognito Tab" and confirm that new tab is opened in
  // incognito with the selected URL.
  [[EarlGrey selectElementWithMatcher:openInNewIncognitoTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::omniboxText(
                                          _URL1.GetContent())]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::AssertMainTabCount(1);
  chrome_test_util::AssertIncognitoTabCount(1);
}

// Tests display and selection of 'Copy URL' in a context menu on a history
// entry.
- (void)testContextMenuCopy {
  ProceduralBlock clearPasteboard = ^{
    [[UIPasteboard generalPasteboard] setURLs:nil];
  };
  [self setTearDownHandler:clearPasteboard];
  clearPasteboard();

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:historyEntryWithURL(_URL1)]
      performAction:grey_longPress()];

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [[EarlGrey selectElementWithMatcher:copyUrlButton()]
      performAction:grey_tap()];
  bool success =
      testing::WaitUntilConditionOrTimeout(testing::kWaitForUIElementTimeout, ^{
        return _URL1 ==
               net::GURLWithNSURL([UIPasteboard generalPasteboard].URL);
      });
  GREYAssertTrue(success, @"Pasteboard URL was not set to %s",
                 _URL1.spec().c_str());
}

// Navigates to history and checks elements for accessibility.
- (void)testAccessibilityOnHistory {
  [self openHistoryPanel];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  // Close history.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::buttonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
}

#pragma mark Helper Methods

- (void)loadTestURLs {
  [ChromeEarlGrey loadURL:_URL1];
  id<GREYMatcher> response1Matcher =
      chrome_test_util::webViewContainingText(kResponse1);
  [[EarlGrey selectElementWithMatcher:response1Matcher]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey loadURL:_URL2];
  id<GREYMatcher> response2Matcher =
      chrome_test_util::webViewContainingText(kResponse2);
  [[EarlGrey selectElementWithMatcher:response2Matcher]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey loadURL:_URL3];
  id<GREYMatcher> response3Matcher =
      chrome_test_util::webViewContainingText(kResponse3);
  [[EarlGrey selectElementWithMatcher:response3Matcher]
      assertWithMatcher:grey_notNil()];
}

- (void)openHistoryPanel {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:historyButton()]
      performAction:grey_tap()];
}

- (void)assertNoHistoryShown {
  id<GREYMatcher> noHistoryMessageMatcher =
      grey_allOf(grey_text(l10n_util::GetNSString(IDS_HISTORY_NO_RESULTS)),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noHistoryMessageMatcher]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> historyEntryMatcher =
      grey_allOf(grey_kindOfClass([HistoryEntryCell class]),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:historyEntryMatcher]
      assertWithMatcher:grey_nil()];
}

- (void)resetBrowsingDataPrefs {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->ClearPref(browsing_data::prefs::kDeleteBrowsingHistory);
  prefs->ClearPref(browsing_data::prefs::kDeleteCookies);
  prefs->ClearPref(browsing_data::prefs::kDeleteCache);
  prefs->ClearPref(browsing_data::prefs::kDeletePasswords);
  prefs->ClearPref(browsing_data::prefs::kDeleteFormData);
}

@end
