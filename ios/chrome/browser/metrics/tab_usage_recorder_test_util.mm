// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/tab_usage_recorder_test_util.h"

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/wait_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The delay to wait for an element to appear before tapping on it.
const NSTimeInterval kWaitElementTimeout = 3;

// Closes the tabs switcher.
void CloseTabSwitcher() {
  id<GREYMatcher> matcher = chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TAB_STRIP_LEAVE_TAB_SWITCHER);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

}  // namespace

namespace tab_usage_recorder_test_util {

void OpenNewIncognitoTabUsingUIAndEvictMainTabs() {
  int nb_incognito_tab = chrome_test_util::GetIncognitoTabCount();
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> new_incognito_tab_button_matcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:new_incognito_tab_button_matcher]
      performAction:grey_tap()];
  chrome_test_util::AssertIncognitoTabCount(nb_incognito_tab + 1);
  ConditionBlock condition = ^bool {
    return chrome_test_util::IsIncognitoMode();
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitElementTimeout, condition),
      @"Waiting switch to incognito mode.");
  chrome_test_util::EvictOtherTabModelTabs();
}

void SwitchToNormalMode() {
  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
                 @"Switching to normal mode is only allowed from Incognito.");
  if (IsIPadIdiom()) {
    // Enter the tab switcher.
    id<GREYMatcher> tabSwitcherEnterButton = grey_accessibilityLabel(
        l10n_util::GetNSStringWithFixup(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER));
    [[EarlGrey selectElementWithMatcher:tabSwitcherEnterButton]
        performAction:grey_tap()];

    // Select the non incognito panel.
    id<GREYMatcher> tabSwitcherHeaderPanelButton =
        grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
            IDS_IOS_TAB_SWITCHER_HEADER_NON_INCOGNITO_TABS));
    [[EarlGrey selectElementWithMatcher:tabSwitcherHeaderPanelButton]
        performAction:grey_tap()];

    // Leave the tab switcher.
    CloseTabSwitcher();
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionRight)];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        performAction:grey_tap()];
  }
  ConditionBlock condition = ^bool {
    return !chrome_test_util::IsIncognitoMode();
  };
  GREYAssert(
      testing::WaitUntilConditionOrTimeout(kWaitElementTimeout, condition),
      @"Waiting switch to normal mode.");
}

}  // namespace tab_usage_recorder_test_util
