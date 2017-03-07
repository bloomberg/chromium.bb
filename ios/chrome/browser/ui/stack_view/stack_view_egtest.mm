// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/stack_view/card_view.h"
#import "ios/chrome/browser/ui/stack_view/stack_view_controller.h"
#import "ios/chrome/browser/ui/stack_view/stack_view_controller_private.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/test/app/stack_view_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/wait_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns a GREYMatcher that matches |view|.
// TODO(crbug.com/642619): Evaluate whether this should be shared code.
id<GREYMatcher> ViewMatchingView(UIView* view) {
  MatchesBlock matches = ^BOOL(UIView* viewToMatch) {
    return viewToMatch == view;
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* matcherDescription =
        [NSString stringWithFormat:@"View matching %@", view];
    [description appendText:matcherDescription];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns a matcher for the StackViewController's view.
id<GREYMatcher> StackView() {
  return ViewMatchingView([chrome_test_util::GetStackViewController() view]);
}

// Waits for the Stack View to be active/inactive.
void WaitForStackViewActive(bool active) {
  NSString* activeStatusString = active ? @"active" : @"inactive";
  NSString* activeTabSwitcherDescription =
      [NSString stringWithFormat:@"Waiting for tab switcher to be %@.",
                                 activeStatusString];
  BOOL (^activeTabSwitcherBlock)
  () = ^BOOL {
    BOOL isActive = chrome_test_util::GetStackViewController() &&
                    chrome_test_util::IsTabSwitcherActive();
    return active ? isActive : !isActive;
  };
  GREYCondition* activeTabSwitcherCondition =
      [GREYCondition conditionWithName:activeTabSwitcherDescription
                                 block:activeTabSwitcherBlock];
  NSString* assertDescription = [NSString
      stringWithFormat:@"Tab switcher did not become %@.", activeStatusString];

  GREYAssert([activeTabSwitcherCondition
                 waitWithTimeout:testing::kWaitForUIElementTimeout],
             assertDescription);
}

// Verify the visibility of the stack view.
void CheckForStackViewVisibility(bool visible) {
  id<GREYMatcher> visibilityMatcher =
      grey_allOf(visible ? grey_sufficientlyVisible() : grey_notVisible(),
                 visible ? grey_notNil() : grey_nil(), nil);
  [[EarlGrey selectElementWithMatcher:StackView()]
      assertWithMatcher:visibilityMatcher];
}

// Opens the StackViewController.
void OpenStackView() {
  if (chrome_test_util::IsTabSwitcherActive())
    return;
  // Tap on the toolbar's tab switcher button.
  id<GREYMatcher> stackButtonMatcher =
      grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:stackButtonMatcher]
      performAction:grey_tap()];
  // Verify that a StackViewController was presented.
  WaitForStackViewActive(true);
  CheckForStackViewVisibility(true);
}

// Shows either the normal or incognito deck.
enum class DeckType : bool { NORMAL, INCOGNITO };
void ShowDeckWithType(DeckType type) {
  OpenStackView();
  StackViewController* stackViewController =
      chrome_test_util::GetStackViewController();
  UIView* activeDisplayView = stackViewController.activeCardSet.displayView;
  // The inactive deck region is in scroll view coordinates, but the tap
  // recognizer is installed on the active card set's display view.
  CGRect inactiveDeckRegion =
      [activeDisplayView convertRect:[stackViewController inactiveDeckRegion]
                            fromView:stackViewController.scrollView];
  bool showIncognito = type == DeckType::INCOGNITO;
  if (showIncognito) {
    GREYAssert(!CGRectIsEmpty(inactiveDeckRegion),
               @"Cannot show Incognito deck if no Incognito tabs are open");
  }
  if (showIncognito != [stackViewController isCurrentSetIncognito]) {
    CGPoint tapPoint = CGPointMake(CGRectGetMidX(inactiveDeckRegion),
                                   CGRectGetMidY(inactiveDeckRegion));
    [[EarlGrey selectElementWithMatcher:ViewMatchingView(activeDisplayView)]
        performAction:grey_tapAtPoint(tapPoint)];
  }
}

// Opens a new tab using the stack view button.
void OpenNewTabUsingStackView() {
  // Open the stack view, tap the New Tab button, and wait for the animation to
  // finish.
  ShowDeckWithType(DeckType::NORMAL);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"New Tab")]
      performAction:grey_tap()];
  WaitForStackViewActive(false);
  CheckForStackViewVisibility(false);
}

// Opens the tools menu from the stack view.
void OpenToolsMenu() {
  OpenStackView();
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
}

// Opens a new Incognito Tab using the stack view button.
void OpenNewIncognitoTabUsingStackView() {
  OpenToolsMenu();
  NSString* newIncognitoTabID = kToolsMenuNewIncognitoTabId;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(newIncognitoTabID)]
      performAction:grey_tap()];
  WaitForStackViewActive(false);
  CheckForStackViewVisibility(false);
}

// Taps the CardView associated with |tab|.
void SelectTabUsingStackView(Tab* tab) {
  DCHECK(tab);
  // Present the StackViewController.
  OpenStackView();
  // Get the StackCard associated with |tab|.
  StackViewController* stackViewController =
      chrome_test_util::GetStackViewController();
  StackCard* nextCard = [[stackViewController activeCardSet] cardForTab:tab];
  UIView* card_title_label = static_cast<UIView*>([[nextCard view] titleLabel]);
  [[EarlGrey selectElementWithMatcher:ViewMatchingView(card_title_label)]
      performAction:grey_tap()];
  // Wait for the StackViewController to be dismissed.
  WaitForStackViewActive(false);
  CheckForStackViewVisibility(false);
  // Checks that the next Tab has been selected.
  GREYAssertEqual(tab, chrome_test_util::GetCurrentTab(),
                  @"The next Tab was not selected");
}
}

// Tests for interacting with the StackViewController.
@interface StackViewTestCase : ChromeTestCase
@end

@implementation StackViewTestCase

// Switches between three Tabs via the stack view.
- (void)testSwitchTabs {
  // The StackViewController is only used on iPhones.
  if (IsIPadIdiom())
    EARL_GREY_TEST_SKIPPED(@"Stack view is not used on iPads.");
  // Open two additional Tabs.
  const NSUInteger kAdditionalTabCount = 2;
  for (NSUInteger i = 0; i < kAdditionalTabCount; ++i)
    OpenNewTabUsingStackView();
  // Select each additional Tab using the stack view UI.
  for (NSUInteger i = 0; i < kAdditionalTabCount + 1; ++i)
    SelectTabUsingStackView(chrome_test_util::GetNextTab());
}

// Tests closing a tab in the stack view.
- (void)testCloseTab {
  // The StackViewController is only used on iPhones.
  if (IsIPadIdiom())
    EARL_GREY_TEST_SKIPPED(@"Stack view is not used on iPads.");
  // Open the stack view and tap the close button on the current CardView.
  OpenStackView();
  StackViewController* stackViewController =
      chrome_test_util::GetStackViewController();
  Tab* currentTab = chrome_test_util::GetCurrentTab();
  StackCard* card = [[stackViewController activeCardSet] cardForTab:currentTab];
  CardView* cardView = card.view;
  NSString* identifier = card.view.closeButtonId;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(identifier)]
      performAction:grey_tap()];
  // Verify that the CardView and its associated Tab were removed.
  [[EarlGrey selectElementWithMatcher:ViewMatchingView(cardView)]
      assertWithMatcher:grey_notVisible()];
  GREYAssertEqual(chrome_test_util::GetMainTabCount(), 0,
                  @"All Tabs should be closed.");
}

// Tests closing all Tabs in the stack view.
- (void)testCloseAllTabs {
  // The StackViewController is only used on iPhones.
  if (IsIPadIdiom())
    EARL_GREY_TEST_SKIPPED(@"Stack view is not used on iPads.");
  // Open an incognito Tab.
  OpenNewIncognitoTabUsingStackView();
  GREYAssertEqual(chrome_test_util::GetIncognitoTabCount(), 1,
                  @"Incognito Tab was not opened.");
  // Open two additional Tabs.
  const NSUInteger kAdditionalTabCount = 2;
  for (NSUInteger i = 0; i < kAdditionalTabCount; ++i)
    OpenNewTabUsingStackView();
  GREYAssertEqual(chrome_test_util::GetMainTabCount(), kAdditionalTabCount + 1,
                  @"Additional Tabs were not opened.");
  // Record all created CardViews.
  OpenStackView();
  StackViewController* stackViewController =
      chrome_test_util::GetStackViewController();
  NSMutableArray* cardViews = [NSMutableArray array];
  for (StackCard* card in [stackViewController activeCardSet].cards) {
    if (card.viewIsLive)
      [cardViews addObject:card.view];
  }
  for (StackCard* card in [stackViewController inactiveCardSet].cards) {
    if (card.viewIsLive)
      [cardViews addObject:card.view];
  }
  // Open the tools menu and select "Close all tabs".
  OpenToolsMenu();
  NSString* closeAllTabsID = kToolsMenuCloseAllTabsId;
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(closeAllTabsID)]
      performAction:grey_tap()];
  // Wait for CardViews to be dismissed.
  for (CardView* cardView in cardViews) {
    [[EarlGrey selectElementWithMatcher:ViewMatchingView(cardView)]
        assertWithMatcher:grey_notVisible()];
  }
  // Check that all Tabs were closed.
  GREYAssertEqual(chrome_test_util::GetMainTabCount(), 0,
                  @"Tabs were not closed.");
  GREYAssertEqual(chrome_test_util::GetIncognitoTabCount(), 0,
                  @"Incognito Tab was not closed.");
}

// Tests that tapping on the inactive deck region switches modes.
- (void)testSwitchingModes {
  // The StackViewController is only used on iPhones.
  if (IsIPadIdiom())
    EARL_GREY_TEST_SKIPPED(@"Stack view is not used on iPads.");
  // Open an Incognito Tab then switch decks.
  OpenNewIncognitoTabUsingStackView();
  ShowDeckWithType(DeckType::INCOGNITO);
  // Verify that the current CardSet is the incognito set.
  StackViewController* stackViewController =
      chrome_test_util::GetStackViewController();
  GREYAssert([stackViewController isCurrentSetIncognito],
             @"Incognito deck not selected.");
  // Switch back to the main CardSet and verify that is selected.
  ShowDeckWithType(DeckType::NORMAL);
  GREYAssert(![stackViewController isCurrentSetIncognito],
             @"Normal deck not selected.");
}

@end
