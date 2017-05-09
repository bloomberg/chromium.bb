// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#import "ios/chrome/browser/ui/util/transparent_link_button.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::WebViewContainingText;

namespace {
char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kTitle1[] = "Page 1";
char kTitle2[] = "Page 2";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";

// Matcher for entry in history for URL and title.
id<GREYMatcher> HistoryEntry(const GURL& url, const std::string& title) {
  NSString* url_spec_text = base::SysUTF8ToNSString(url.spec());
  NSString* title_text = base::SysUTF8ToNSString(title);

  MatchesBlock matches = ^BOOL(HistoryEntryCell* cell) {
    return [cell.textLabel.text isEqual:title_text] &&
           [cell.detailTextLabel.text isEqual:url_spec_text];
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"view containing URL text: "];
    [description appendText:url_spec_text];
    [description appendText:@" title text: "];
    [description appendText:title_text];
  };

  return grey_allOf(
      grey_kindOfClass([HistoryEntryCell class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      grey_sufficientlyVisible(), nil);
}
// Matcher for the history button in the tools menu.
id<GREYMatcher> HistoryButton() {
  return ButtonWithAccessibilityLabelId(IDS_HISTORY_SHOW_HISTORY);
}
// Matcher for the edit button in the navigation bar.
id<GREYMatcher> NavigationEditButton() {
  return ButtonWithAccessibilityLabelId(IDS_HISTORY_START_EDITING_BUTTON);
}
// Matcher for the delete button.
id<GREYMatcher> DeleteHistoryEntriesButton() {
  // Include class restriction to exclude MDCCollectionViewInfoBar, which is
  // hidden.
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_HISTORY_DELETE_SELECTED_ENTRIES_BUTTON),
                    grey_kindOfClass([UIButton class]), nil);
}
// Matcher for the search button.
id<GREYMatcher> SearchIconButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ICON_SEARCH);
}
// Matcher for the cancel button.
id<GREYMatcher> CancelButton() {
  return ButtonWithAccessibilityLabelId(IDS_HISTORY_CANCEL_EDITING_BUTTON);
}
// Matcher for the button to open the clear browsing data panel.
id<GREYMatcher> OpenClearBrowsingDataButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG);
}
// Matcher for the Open in New Incognito Tab option in the context menu.
id<GREYMatcher> OpenInNewIncognitoTabButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
}
// Matcher for the Copy URL option in the context menu.
id<GREYMatcher> CopyUrlButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_COPY);
}
// Matcher for the clear cookies cell on the clear browsing data panel.
id<GREYMatcher> ClearCookiesButton() {
  return grey_accessibilityID(kClearCookiesCellId);
}
// Matcher for the clear cache cell on the clear browsing data panel.
id<GREYMatcher> ClearCacheButton() {
  return grey_allOf(grey_accessibilityID(kClearCacheCellId),
                    grey_sufficientlyVisible(), nil);
}
// Matcher for the clear browsing data button on the clear browsing data panel.
id<GREYMatcher> ClearBrowsingDataButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CLEAR_BUTTON);
}
// Matcher for the clear browsing data action sheet item.
id<GREYMatcher> ConfirmClearBrowsingDataButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONFIRM_CLEAR_BUTTON);
}

// Sign in with a mock identity.
void MockSignIn() {
  // Set up a mock identity.
  ChromeIdentity* identity =
      [FakeChromeIdentity identityWithEmail:@"foo@gmail.com"
                                     gaiaID:@"fooID"
                                       name:@"Fake Foo"];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsSignInCellId)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   identity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
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
  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  responses[web::test::HttpServer::MakeUrl(kURL1)] =
      base::StringPrintf(kPageFormat, kTitle1, kResponse1);
  responses[web::test::HttpServer::MakeUrl(kURL2)] =
      base::StringPrintf(kPageFormat, kTitle2, kResponse2);
  // Page 3 does not have <title> tag, so URL will be its title.
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
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()
                                                              error:&error];
  // Dismiss history panel by pressing done, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
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
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap a history entry and assert that navigation to that entry's URL occurs.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kResponse1)]
      assertWithMatcher:grey_notNil()];
}

// Tests that history is not changed after performing back navigation.
- (void)testHistoryUpdateAfterBackNavigation {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey loadURL:_URL2];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kResponse1)]
      assertWithMatcher:grey_notNil()];

  [self openHistoryPanel];

  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
}

// Test that history displays a message about entries only if the user is logged
// in, and that tapping on the link in the message opens a new tab with the sync
// help page.
- (void)testHistoryEntriesStatusCell {
  [self loadTestURLs];
  [self openHistoryPanel];
  // Assert that no message is shown when the user is not signed in.
  NSRange range;
  NSString* entriesMessage = ParseStringWithLink(
      l10n_util::GetNSString(IDS_IOS_HISTORY_NO_SYNCED_RESULTS), &range);
  [[EarlGrey selectElementWithMatcher:grey_text(entriesMessage)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Sign in and assert that the page indicates what type of history entries
  // are shown.
  MockSignIn();
  [self openHistoryPanel];
  // Assert that message about entries is shown. The "history is showing local
  // entries" message will be shown because sync is not set up for this test.
  [[EarlGrey selectElementWithMatcher:grey_text(entriesMessage)]
      assertWithMatcher:grey_notNil()];

  // Tap on "Learn more" link.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClass([TransparentLinkButton class])]
      performAction:grey_tap()];
  // Assert that new tab with the link is opened, hence tab count of 2.
  chrome_test_util::AssertMainTabCount(2);
}

// Tests that searching history displays only entries matching the search term.
- (void)testSearchHistory {
  [self loadTestURLs];
  [self openHistoryPanel];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  NSString* searchString =
      [NSString stringWithFormat:@"%s", _URL1.path().c_str()];
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_typeText(searchString)];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_nil()];
}

// Tests deletion of history entries.
- (void)testDeleteHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that three history elements are present.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select a history element, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select both remaining entries, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  [self assertNoHistoryShown];
}

// Tests clear browsing history.
- (void)testClearBrowsingHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Open the Clear Browsing Data dialog.
  [[EarlGrey selectElementWithMatcher:OpenClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Uncheck "Cookies, Site Data" and "Cached Images and Files," which are
  // checked by default, and press "Clear Browsing Data"
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Include sufficientlyVisible condition for the case of the clear browsing
  // dialog, which also has a "Done" button and is displayed over the history
  // panel.
  id<GREYMatcher> visibleDoneButton =
      grey_allOf(chrome_test_util::NavigationBarDoneButton(),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:visibleDoneButton]
      performAction:grey_tap()];

  [self assertNoHistoryShown];
}

// Tests display and selection of 'Open in New Tab' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewTab {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
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
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Select "Open in New Incognito Tab" and confirm that new tab is opened in
  // incognito with the selected URL.
  [[EarlGrey selectElementWithMatcher:OpenInNewIncognitoTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
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
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [[EarlGrey selectElementWithMatcher:CopyUrlButton()]
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
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

#pragma mark Helper Methods

- (void)loadTestURLs {
  [ChromeEarlGrey loadURL:_URL1];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kResponse1)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey loadURL:_URL2];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kResponse2)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey loadURL:_URL3];
  [[EarlGrey selectElementWithMatcher:WebViewContainingText(kResponse3)]
      assertWithMatcher:grey_notNil()];
}

- (void)openHistoryPanel {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:HistoryButton()]
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
