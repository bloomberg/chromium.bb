// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_egtest_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_cell.h"
#import "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::TabletTabSwitcherCloseButton;
using chrome_test_util::TabletTabSwitcherIncognitoTabsPanelButton;
using chrome_test_util::TabletTabSwitcherNewIncognitoTabButton;
using chrome_test_util::TabletTabSwitcherNewTabButton;
using chrome_test_util::TabletTabSwitcherOpenButton;
using chrome_test_util::TabletTabSwitcherOpenTabsPanelButton;

namespace {

// Returns the tab model for non-incognito tabs.
TabModel* GetNormalTabModel() {
  return [[chrome_test_util::GetMainController() browserViewInformation]
      mainTabModel];
}

// Shows the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
void ShowTabSwitcher() {
  id<GREYMatcher> matcher = nil;
  if (IsIPadIdiom()) {
    matcher = TabletTabSwitcherOpenButton();
  } else {
    matcher = chrome_test_util::ShowTabsButton();
  }
  DCHECK(matcher);

  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Hides the tab switcher by tapping the switcher button.  Works on both phone
// and tablet.
void ShowTabViewController() {
  id<GREYMatcher> matcher = nil;
  if (IsIPadIdiom()) {
    matcher = TabletTabSwitcherCloseButton();
  } else {
    matcher = chrome_test_util::ShowTabsButton();
  }
  DCHECK(matcher);

  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Selects and focuses the tab with the given |title|.
void SelectTab(NSString* title) {
  if (IsIPadIdiom()) {
    // The UILabel containing the tab's title is not tappable, but its parent
    // TabSwitcherLocalSessionCell is.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_kindOfClass(
                                         [TabSwitcherLocalSessionCell class]),
                                     grey_descendant(grey_text(title)), nil)]
        performAction:grey_tap()];
  } else {
    // On phone, tapping on the title UILabel will select the tab.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(title),
                                            grey_accessibilityTrait(
                                                UIAccessibilityTraitStaticText),
                                            nil)] performAction:grey_tap()];
  }
}

// net::EmbeddedTestServer handler that responds with the request's query as the
// title.
std::unique_ptr<net::test_server::HttpResponse> HandleQueryTitle(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content("<html><head><title>" + request.GetURL().query() +
                             "</title></head></html>");
  return std::move(http_response);
}

}  // namespace

@interface TabSwitcherTransitionTestCase : ChromeTestCase
@end

// NOTE: The test cases before are not totally independent.  For example, the
// setup steps for testEnterTabSwitcherWithOneIncognitoTab first close the last
// normal tab and then open a new incognito tab, which are both scenarios
// covered by other tests.  A single programming error may cause multiple tests
// to fail.
@implementation TabSwitcherTransitionTestCase

// Sets up the EmbeddedTestServer as needed for tests.
- (void)setUpTestServer {
  self.testServer->RegisterDefaultHandler(
      base::Bind(net::test_server::HandlePrefixedRequest, "/querytitle",
                 base::Bind(&HandleQueryTitle)));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start");
}

// Returns the URL for a test page with the given |title|.
- (GURL)makeURLForTitle:(NSString*)title {
  return self.testServer->GetURL("/querytitle?" +
                                 base::SysNSStringToUTF8(title));
}

// Tests entering the tab switcher when one normal tab is open.
- (void)testEnterSwitcherWithOneNormalTab {
  ShowTabSwitcher();
}

// Tests entering the tab switcher when more than one normal tab is open.
- (void)testEnterSwitcherWithMultipleNormalTabs {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openNewTab];

  ShowTabSwitcher();
}

// Tests entering the tab switcher when one incognito tab is open.
- (void)testEnterSwitcherWithOneIncognitoTab {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];

  ShowTabSwitcher();
}

// Tests entering the tab switcher when more than one incognito tab is open.
- (void)testEnterSwitcherWithMultipleIncognitoTabs {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewIncognitoTab];

  ShowTabSwitcher();
}

// Tests entering the switcher when multiple tabs of both types are open.
- (void)testEnterSwitcherWithNormalAndIncognitoTabs {
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewIncognitoTab];

  ShowTabSwitcher();
}

// Tests entering the tab switcher by closing the last normal tab.
- (void)testEnterSwitcherByClosingLastNormalTab {
  chrome_test_util::CloseAllTabsInCurrentMode();
}

// Tests entering the tab switcher by closing the last incognito tab.
- (void)testEnterSwitcherByClosingLastIncognitoTab {
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];

  chrome_test_util::CloseAllTabsInCurrentMode();
}

// Tests exiting the switcher by tapping the switcher button.
- (void)testLeaveSwitcherWithSwitcherButton {
  ShowTabSwitcher();
  ShowTabViewController();
}

// Tests exiting the switcher by tapping the new tab button or selecting new tab
// from the menu (on phone only).
- (void)testLeaveSwitcherByOpeningNewNormalTab {
  // Enter the switcher and open a new tab using the new tab button.
  ShowTabSwitcher();
  if (IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:TabletTabSwitcherNewTabButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                            IDS_IOS_TOOLS_MENU_NEW_TAB)]
        performAction:grey_tap()];
  }

  if (!IsIPadIdiom()) {
    // On phone, enter the switcher again and open a new tab using the tools
    // menu.  This does not apply on tablet because there is no tools menu
    // in the switcher.
    ShowTabSwitcher();
    [ChromeEarlGreyUI openNewTab];
  }
}

// Tests exiting the switcher by tapping the new incognito tab button or
// selecting new incognito tab from the menu (on phone only).
- (void)testLeaveSwitcherByOpeningNewIncognitoTab {
  // Set up by creating a new incognito tab and closing all normal tabs.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [GetNormalTabModel() closeAllTabs];

  // Enter the switcher and open a new incognito tab using the new tab button.
  ShowTabSwitcher();
  if (IsIPadIdiom()) {
    [[EarlGrey
        selectElementWithMatcher:TabletTabSwitcherNewIncognitoTabButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey
        selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                     IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)]
        performAction:grey_tap()];
  }

  if (!IsIPadIdiom()) {
    // On phone, enter the switcher again and open a new incognito tab using the
    // tools menu.  This does not apply on tablet because there is no tools menu
    // in the switcher.
    ShowTabSwitcher();
    [ChromeEarlGreyUI openNewIncognitoTab];
  }
}

// Tests exiting the switcher by opening a new tab in the other tab model.
- (void)testLeaveSwitcherByOpeningTabInOtherMode {
  // Go from normal mode to incognito mode.
  ShowTabSwitcher();
  if (IsIPadIdiom()) {
    [[EarlGrey
        selectElementWithMatcher:TabletTabSwitcherIncognitoTabsPanelButton()]
        performAction:grey_tap()];
    [[EarlGrey
        selectElementWithMatcher:TabletTabSwitcherNewIncognitoTabButton()]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openNewIncognitoTab];
  }

  // Go from incognito mode to normal mode.
  ShowTabSwitcher();
  if (IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:TabletTabSwitcherOpenTabsPanelButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:TabletTabSwitcherNewTabButton()]
        performAction:grey_tap()];
  } else {
    [ChromeEarlGreyUI openNewTab];
  }
}

// Tests exiting the tab switcher by selecting a normal tab.
- (void)testLeaveSwitcherBySelectingNormalTab {
  NSString* tab1_title = @"NormalTab1";
  NSString* tab2_title = @"NormalTab2";
  NSString* tab3_title = @"NormalTab3";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab2_title]];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab3_title]];

  ShowTabSwitcher();
  SelectTab(tab1_title);

  ShowTabSwitcher();
  SelectTab(tab3_title);
}

// Tests exiting the tab switcher by selecting an incognito tab.
- (void)testLeaveSwitcherBySelectingIncognitoTab {
  NSString* tab1_title = @"IncognitoTab1";
  NSString* tab2_title = @"IncognitoTab2";
  NSString* tab3_title = @"IncognitoTab3";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab1_title]];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab2_title]];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:tab3_title]];
  [GetNormalTabModel() closeAllTabs];

  ShowTabSwitcher();
  SelectTab(tab1_title);

  ShowTabSwitcher();
  SelectTab(tab3_title);
}

// Tests exiting the tab switcher by selecting a tab in the other tab model.
- (void)testLeaveSwitcherBySelectingTabInOtherMode {
  NSString* normal_title = @"NormalTab";
  NSString* incognito_title = @"IncognitoTab";
  [self setUpTestServer];

  // Create a few tabs and give them all unique titles.
  [ChromeEarlGrey loadURL:[self makeURLForTitle:normal_title]];
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGrey loadURL:[self makeURLForTitle:incognito_title]];

  ShowTabSwitcher();
  if (IsIPadIdiom()) {
    // On tablet, switch to the normal panel and select the one tab that is
    // there.
    [[EarlGrey selectElementWithMatcher:TabletTabSwitcherOpenTabsPanelButton()]
        performAction:grey_tap()];
  } else {
    // On phone, get to the normal card stack by swiping right on the current
    // incognito card.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityLabel(incognito_title),
                                     grey_accessibilityTrait(
                                         UIAccessibilityTraitStaticText),
                                     nil)]
        performAction:grey_swipeFastInDirection(kGREYDirectionRight)];
  }
  SelectTab(normal_title);

  ShowTabSwitcher();
  if (IsIPadIdiom()) {
    // On tablet, switch to the incognito panel and select the one tab that is
    // there.
    [[EarlGrey
        selectElementWithMatcher:TabletTabSwitcherIncognitoTabsPanelButton()]
        performAction:grey_tap()];
  } else {
    // On phone, get to the incognito card stack by swiping left on the current
    // normal card.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityLabel(normal_title),
                                     grey_accessibilityTrait(
                                         UIAccessibilityTraitStaticText),
                                     nil)]
        performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];
  }
  SelectTab(incognito_title);
}

// Tests switching back and forth between the normal and incognito BVCs.
- (void)testSwappingBVCModesWithoutEnteringSwitcher {
  // Opening a new tab from the menu will force a change in BVC.
  [ChromeEarlGreyUI openNewIncognitoTab];
  [ChromeEarlGreyUI openNewTab];
}

@end
