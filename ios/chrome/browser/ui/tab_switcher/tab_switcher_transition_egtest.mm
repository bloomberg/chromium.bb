// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_egtest_util.h"
#import "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

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

}  // namespace

@interface TabSwitcherTransitionTestCase : ChromeTestCase
@end

// NOTE: The test cases before are not totally independent.  For example, the
// setup steps for testEnterTabSwitcherWithOneIncognitoTab first close the last
// normal tab and then open a new incognito tab, which are both scenarios
// covered by other tests.  A single programming error may cause multiple tests
// to fail.
@implementation TabSwitcherTransitionTestCase

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

- (void)testLeaveSwitcherWithSwitcherButton {
  ShowTabSwitcher();
  ShowTabViewController();
}

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

@end
