// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_row.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;

// Toolbar integration tests for Chrome.
@interface ToolbarTestCase : ChromeTestCase
@end

namespace {

// Displays the |panel_type| new tab page.  On a phone this will send a command
// to display a dialog, on tablet this calls -selectPanel to slide the NTP.
void SelectNewTabPagePanel(ntp_home::PanelIdentifier panel_type) {
  NewTabPageController* ntp_controller =
      chrome_test_util::GetCurrentNewTabPageController();
  if (IsIPadIdiom()) {
    [ntp_controller selectPanel:panel_type];
  } else if (panel_type == ntp_home::BOOKMARKS_PANEL) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC()
        showBookmarksManager];
  } else if (panel_type == ntp_home::RECENT_TABS_PANEL) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() showRecentTabs];
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

}  // namespace

@implementation ToolbarTestCase

#pragma mark Tests

// Verifies that entering a URL in the omnibox navigates to the correct URL and
// displays content.
- (void)testEnterURL {
  web::test::SetUpFileBasedHttpServer();
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:URL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebViewContainingText:"You've arrived"];
}

// Verifies opening a new tab from the tools menu.
- (void)testNewTabFromMenu {
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open tab via the UI.
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewTabId);
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];
}

// Verifies opening a new incognito tab from the tools menu.
// TODO(crbug.com/631078): Enable this test.
- (void)FLAKY_testNewIncognitoTabFromMenu {
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Open incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newIncognitoTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabButtonMatcher]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Tests whether input mode in an omnibox can be canceled via "Cancel" button
// and asserts it doesn't commit the omnibox contents if the input is canceled.
- (void)testToolbarOmniboxCancel {
  // Handset only (tablet does not have cancel button).
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not support on iPad");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  id<GREYMatcher> cancelButton =
      grey_allOf(chrome_test_util::CancelButton(),
                 grey_not(grey_accessibilityID(@"Typing Shield")), nil);
  [[EarlGrey selectElementWithMatcher:cancelButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
}

// Tests whether input mode in an omnibox can be canceled via "hide keyboard"
// button and asserts it doesn't commit the omnibox contents if the input is
// canceled.
- (void)testToolbarOmniboxHideKeyboard {
  // TODO(crbug.com/642559): Enable the test for iPad when typing bug is fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to a simulator bug.");
  }

  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not support on iPhone");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  id<GREYMatcher> hideKeyboard = grey_accessibilityLabel(@"Hide keyboard");
  [[EarlGrey selectElementWithMatcher:hideKeyboard] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
}

// Tests whether input mode in an omnibox can be canceled via tapping the typing
// shield and asserts it doesn't commit the omnibox contents if the input is
// canceled.
- (void)testToolbarOmniboxTypingShield {
  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not support on iPhone");
  }

  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  id<GREYMatcher> typingShield = grey_accessibilityID(@"Typing Shield");
  [[EarlGrey selectElementWithMatcher:typingShield] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
}

// Verifies the existence and state of toolbar UI elements.
- (void)testToolbarUI {
  id<GREYMatcher> reloadButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_RELOAD);
  id<GREYMatcher> bookmarkButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
  id<GREYMatcher> voiceSearchButton =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCNAME_VOICE_SEARCH),
                 grey_ancestor(grey_kindOfClass([ToolbarView class])), nil);
  NSString* ntpOmniboxLabel = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  NSString* focusedOmniboxLabel = l10n_util::GetNSString(IDS_ACCNAME_LOCATION);
  NSString* omniboxLabel =
      IsIPadIdiom() ? focusedOmniboxLabel : ntpOmniboxLabel;
  id<GREYMatcher> locationbarButton =
      grey_allOf(grey_accessibilityLabel(omniboxLabel),
                 grey_minimumVisiblePercent(0.2), nil);

  [[EarlGrey selectElementWithMatcher:locationbarButton]
      assertWithMatcher:grey_sufficientlyVisible()];

  if (IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:reloadButton]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:bookmarkButton]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:voiceSearchButton]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Navigate to a page and verify the back button is enabled.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
}

// Verifies that the keyboard is properly dismissed when a toolbar button
// is pressed (iPad specific).
- (void)testIPadKeyboardDismissOnButtonPress {
  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not supported on iPhone");
  }

  // Load some page so that the "Back" button is tappable.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // First test: check that the keyboard is opened when tapping the omnibox,
  // and that it is dismissed when the "Back" button is tapped.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notVisible()];

  // Second test: check that the keyboard is opened when tapping the omnibox,
  // and that it is dismissed when the tools menu button is tapped.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that copying and pasting a URL includes the hidden protocol prefix.
- (void)testCopyPasteURL {
  // TODO(crbug.com/686069): Re-enable this test.  It is failing on iOS 9.
  if (!base::ios::IsRunningOnIOS10OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iOS 9.");
  }

  // Clear generalPasteboard before and after the test.
  [UIPasteboard generalPasteboard].string = @"";
  [self setTearDownHandler:^{
    [UIPasteboard generalPasteboard].string = @"";
  }];

  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://testPage");
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://pastePage");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];

  if (base::ios::IsRunningOnIOS11OrLater()) {
    // Can't access share menu from xctest on iOS 11+, so use the text field
    // callout bar instead.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_tap()];
    // Tap twice to get the pre-edit label callout bar copy button.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_tap()];

    [[[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Copy")]
        inRoot:grey_kindOfClass(NSClassFromString(@"UICalloutBarButton"))]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openShareMenu];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                     @"Copy")] performAction:grey_tap()];
  }

  [ChromeEarlGrey loadURL:secondURL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  id<GREYMatcher> pasteBarButton = grey_allOf(
      grey_accessibilityLabel(@"Paste"),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitStaticText)), nil);
  [[EarlGrey selectElementWithMatcher:pasteBarButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.spec().c_str())];
}

// Verifies that the clear text button clears any text in the omnibox.
- (void)testOmniboxClearTextButton {
  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("foo")];

  id<GREYMatcher> CancelButton = grey_accessibilityLabel(@"Clear Text");
  [[EarlGrey selectElementWithMatcher:CancelButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];
}

// Types JavaScript into Omnibox and verify that an alert is displayed.
- (void)testTypeJavaScriptIntoOmnibox {
  // TODO(crbug.com/642544): Enable the test for iPad when typing bug is fixed.
  if (IsIPadIdiom() && base::ios::IsRunningOnIOS10OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iOS10 iPad due to a typing bug.");
  }

  std::map<GURL, std::string> responses;
  GURL URL = web::test::HttpServer::MakeUrl("http://foo");
  responses[URL] = "bar";
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:GURL(URL)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"javascript:alert('Hello');")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Hello")]
      assertWithMatcher:grey_notNil()];
}

// Loads WebUI page, types JavaScript into Omnibox and verifies that alert is
// not displayed. WebUI pages have elevated privileges and should not allow
// script execution.
- (void)testTypeJavaScriptIntoOmniboxWithWebUIPage {
  // TODO(crbug.com/642544): Enable the test for iPad when typing bug is fixed.
  if (IsIPadIdiom() && base::ios::IsRunningOnIOS10OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iOS10 iPad due to a typing bug.");
  }
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"javascript:alert('Hello');")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Hello")]
      assertWithMatcher:grey_nil()];
}

// Tests typing in the omnibox.
- (void)testToolbarOmniboxTyping {
  // TODO(crbug.com/642559): Enable this test for iPad when typing bug is fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to a simulator bug.");
  }
  SelectNewTabPagePanel(ntp_home::HOME_PANEL);

  id<GREYMatcher> locationbarButton = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
  [[EarlGrey selectElementWithMatcher:locationbarButton]
      assertWithMatcher:grey_text(@"Search or type URL")];

  [[EarlGrey selectElementWithMatcher:locationbarButton]
      performAction:grey_typeText(@"a")];
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"a"),
                                                 grey_kindOfClass(
                                                     [OmniboxPopupRow class]),
                                                 nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"b")];
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"ab"),
                                                 grey_kindOfClass(
                                                     [OmniboxPopupRow class]),
                                                 nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"C")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"1")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC1"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"2")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"@")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12@"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"{")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12@{"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"#")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12@{#"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  NSString* cancelButtonText = l10n_util::GetNSString(IDS_CANCEL);
  NSString* typingShield = @"Hide keyboard";
  NSString* clearText = IsIPadIdiom() ? typingShield : cancelButtonText;

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(clearText)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];

  SelectNewTabPagePanel(ntp_home::HOME_PANEL);
}

// Tests typing in the omnibox using the keyboard accessory view.
- (void)testToolbarOmniboxKeyboardAccessoryView {
  // Select the omnibox to get the keyboard up.
  id<GREYMatcher> locationbarButton = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
  [[EarlGrey selectElementWithMatcher:locationbarButton]
      assertWithMatcher:grey_text(@"Search or type URL")];
  [[EarlGrey selectElementWithMatcher:locationbarButton]
      performAction:grey_tap()];

  // Tap the "/" keyboard accessory button.
  id<GREYMatcher> slashButtonMatcher = grey_allOf(
      grey_accessibilityLabel(@"/"), grey_kindOfClass([UIButton class]), nil);

  [[EarlGrey selectElementWithMatcher:slashButtonMatcher]
      performAction:grey_tap()];

  // Tap the ".com" keyboard accessory button.
  id<GREYMatcher> dotComButtonMatcher =
      grey_allOf(grey_accessibilityLabel(@".com"),
                 grey_kindOfClass([UIButton class]), nil);

  [[EarlGrey selectElementWithMatcher:dotComButtonMatcher]
      performAction:grey_tap()];

  // Verify that the omnibox contains "/.com"
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"/.com"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
