// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_cell.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server/blank_page_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using web::test::HttpServer;

namespace {

// Returns the GREYMatcher for the button that opens the tab switcher.
id<GREYMatcher> TabSwitcherOpenButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER);
}
// Returns the GREYMatcher for the button that closes the tab switcher.
id<GREYMatcher> TabSwitcherCloseButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TAB_STRIP_LEAVE_TAB_SWITCHER);
}
// Returns the GREYMatcher for the incognito tabs button in the tab switcher.
id<GREYMatcher> TabSwitcherIncognitoButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_TAB_SWITCHER_HEADER_INCOGNITO_TABS);
}
// Returns the GREYMatcher for the incognito tabs button in the tab switcher.
id<GREYMatcher> TabSwitcherOtherDevicesButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_TAB_SWITCHER_HEADER_OTHER_DEVICES_TABS);
}

// Returns the GREYMatcher for the button that creates new non incognito tabs
// from within the tab switcher.
id<GREYMatcher> TabSwitcherNewTabButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_TAB_SWITCHER_CREATE_NEW_TAB),
      grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the button that creates new incognito tabs from
// within the tab switcher.
id<GREYMatcher> TabSwitcherNewIncognitoTabButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_TAB_SWITCHER_CREATE_NEW_INCOGNITO_TAB),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the button to go to the non incognito panel in
// the tab switcher.
id<GREYMatcher> TabSwitcherHeaderPanelButton() {
  NSString* accessibility_label = l10n_util::GetNSStringWithFixup(
      IDS_IOS_TAB_SWITCHER_HEADER_NON_INCOGNITO_TABS);
  return grey_accessibilityLabel(accessibility_label);
}

// Returns the GREYMatcher for the button that closes tabs on iPad.
id<GREYMatcher> CloseTabButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_CLOSE_TAB);
}

// Opens a new incognito tabs using the tools menu.
void OpenNewIncognitoTabUsingUI() {
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newIncognitoTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabButtonMatcher]
      performAction:grey_tap()];
}

// Triggers the opening of the tab switcher by launching a command. Should be
// called only when the tab switcher is not presented.
void EnterTabSwitcherWithCommand() {
  [[EarlGrey selectElementWithMatcher:TabSwitcherOpenButton()]
      performAction:grey_tap()];
}

}  // namespace

@interface TabSwitcherControllerTestCase : ChromeTestCase
@end

@implementation TabSwitcherControllerTestCase

// Checks that the tab switcher is not presented.
- (void)assertTabSwitcherIsInactive {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  MainController* main_controller = chrome_test_util::GetMainController();
  GREYAssertTrue(![main_controller isTabSwitcherActive],
                 @"Tab Switcher should be inactive");
}

// Checks that the tab switcher is active.
- (void)assertTabSwitcherIsActive {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  MainController* main_controller = chrome_test_util::GetMainController();
  GREYAssertTrue([main_controller isTabSwitcherActive],
                 @"Tab Switcher should be active");
}

// Checks that the text associated with |messageId| is somewhere on screen.
- (void)assertMessageIsVisible:(int)messageId {
  id<GREYMatcher> messageMatcher =
      grey_allOf(StaticTextWithAccessibilityLabelId(messageId),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_notNil()];
}

// Checks that the text associated with |messageId| is not visible.
- (void)assertMessageIsNotVisible:(int)messageId {
  id<GREYMatcher> messageMatcher =
      grey_allOf(StaticTextWithAccessibilityLabelId(messageId),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_nil()];
}

// Tests entering and leaving the tab switcher.
- (void)testEnteringTabSwitcher {
  if (!IsIPadIdiom())
    return;

  [self assertTabSwitcherIsInactive];

  EnterTabSwitcherWithCommand();
  [self assertTabSwitcherIsActive];

  // Check that the "No Open Tabs" message is not displayed.
  [self assertMessageIsNotVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  // Press the :: icon to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:TabSwitcherCloseButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests entering tab switcher by closing all tabs, and leaving the tab switcher
// by creating a new tab.
- (void)testClosingAllTabsAndCreatingNewTab {
  if (!IsIPadIdiom())
    return;

  [self assertTabSwitcherIsInactive];

  // Close the tab.
  [[EarlGrey selectElementWithMatcher:CloseTabButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsActive];

  // Check that the "No Open Tabs" message is displayed.
  [self assertMessageIsVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  // Create a new tab.
  [[EarlGrey selectElementWithMatcher:TabSwitcherNewTabButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests entering tab switcher from incognito mode.
- (void)testIncognitoTabs {
  if (!IsIPadIdiom())
    return;

  [self assertTabSwitcherIsInactive];

  // Create new incognito tab from tools menu.
  OpenNewIncognitoTabUsingUI();

  // Close the incognito tab and check that the we are entering the tab
  // switcher.
  [[EarlGrey selectElementWithMatcher:CloseTabButton()]
      performAction:grey_tap()];
  [self assertTabSwitcherIsActive];

  // Check that the "No Incognito Tabs" message is shown.
  [self assertMessageIsVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_INCOGNITO_TABS_PROMO];

  // Create new incognito tab.
  [[EarlGrey selectElementWithMatcher:TabSwitcherNewIncognitoTabButton()]
      performAction:grey_tap()];

  // Verify that we've left the tab switcher.
  [self assertTabSwitcherIsInactive];

  // Close tab and verify we've entered the tab switcher again.
  [[EarlGrey selectElementWithMatcher:CloseTabButton()]
      performAction:grey_tap()];
  [self assertTabSwitcherIsActive];

  // Switch to the non incognito panel.
  [[EarlGrey selectElementWithMatcher:TabSwitcherHeaderPanelButton()]
      performAction:grey_tap()];

  // Press the :: icon to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:TabSwitcherCloseButton()]
      performAction:grey_tap()];

  // Verify that we've left the tab switcher.
  [self assertTabSwitcherIsInactive];
}

// Tests that elements on iPad tab switcher are accessible.
// TODO: (crbug.com/691095) Open tabs label is not accessible
- (void)DISABLED_testAccessibilityOnTabSwitcher {
  if (!IsIPadIdiom())
    return;
  [self assertTabSwitcherIsInactive];

  EnterTabSwitcherWithCommand();
  [self assertTabSwitcherIsActive];
  // Check that the "No Open Tabs" message is not displayed.
  [self assertMessageIsNotVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Press the :: icon to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:TabSwitcherCloseButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests that elements on iPad tab switcher incognito tab are accessible.
// TODO: (crbug.com/691095) Incognito tabs label should be tappable.
- (void)DISABLED_testAccessibilityOnIncognitoTabSwitcher {
  if (!IsIPadIdiom())
    return;
  [self assertTabSwitcherIsInactive];

  EnterTabSwitcherWithCommand();
  [self assertTabSwitcherIsActive];
  // Check that the "No Open Tabs" message is not displayed.
  [self assertMessageIsNotVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  // Press incognito tabs button.
  [[EarlGrey selectElementWithMatcher:TabSwitcherIncognitoButton()]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Press the :: icon to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:TabSwitcherCloseButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests that elements on iPad tab switcher other devices are accessible.
// TODO: (crbug.com/691095) Other devices label should be tappable.
- (void)DISABLED_testAccessibilityOnOtherDeviceTabSwitcher {
  if (!IsIPadIdiom())
    return;
  [self assertTabSwitcherIsInactive];

  EnterTabSwitcherWithCommand();
  [self assertTabSwitcherIsActive];
  // Check that the "No Open Tabs" message is not displayed.
  [self assertMessageIsNotVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  // Press other devices button.
  [[EarlGrey selectElementWithMatcher:TabSwitcherOtherDevicesButton()]
      performAction:grey_tap()];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Create new incognito tab to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:TabSwitcherNewIncognitoTabButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests that closing a Tab that has a queued dialog successfully cancels the
// dialog.
- (void)testCloseTabWithDialog {
  // The TabSwitcherController is only used on iPhones.
  if (!IsIPadIdiom())
    EARL_GREY_TEST_SKIPPED(@"TabSwitcherController is only used on iPads.");

  // Load the blank test page so that JavaScript can be executed.
  const GURL kBlankPageURL = HttpServer::MakeUrl("http://blank-page");
  web::test::AddResponseProvider(
      web::test::CreateBlankPageResponseProvider(kBlankPageURL));
  [ChromeEarlGrey loadURL:kBlankPageURL];

  // Enter the tab switcher and show a dialog from the test page.
  EnterTabSwitcherWithCommand();
  NSString* const kCancelledMessageText = @"CANCELLED";
  NSString* const kAlertFormat = @"alert(\"%@\");";
  chrome_test_util::ExecuteJavaScript(
      [NSString stringWithFormat:kAlertFormat, kCancelledMessageText], nil);

  // Close the tab so that the queued dialog is cancelled.
  [[self class] closeAllTabs];

  // Open a new tab.  This will exit the stack view and will make the non-
  // incognito BrowserState active.  Attempt to present an alert with
  // kMessageText to verify that there aren't any alerts still in the queue.
  [[EarlGrey selectElementWithMatcher:TabSwitcherNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey loadURL:kBlankPageURL];
  NSString* const kMessageText = @"MESSAGE";
  chrome_test_util::ExecuteJavaScript(
      [NSString stringWithFormat:kAlertFormat, kMessageText], nil);

  // Wait for an alert with kMessageText.  If it is shown, then the dialog using
  // kCancelledMessageText for the message was properly cancelled when the Tab
  // was closed.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(kMessageText);
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:messageLabel]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return !error;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout, condition),
             @"Alert with message was not found: %@", kMessageText);
}

@end
