// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_factory.h"

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/content_suggestions/content_suggestions_alert_commands.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsAlertFactoryTestCase : XCTestCase

@end

@implementation ContentSuggestionsAlertFactoryTestCase

- (void)testSuggestionsAlert {
  UIViewController* viewController =
      top_view_controller::TopPresentedViewController();
  AlertCoordinator* coordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForSuggestionItem:nil
                       onViewController:viewController
                                atPoint:CGPointMake(50, 50)
                            atIndexPath:nil
                         commandHandler:nil];
  [coordinator start];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_APP_CANCEL)]
      assertWithMatcher:grey_interactable()];

  [coordinator stop];
}

- (void)testMostVisitedAlert {
  UIViewController* viewController =
      top_view_controller::TopPresentedViewController();
  AlertCoordinator* coordinator = [ContentSuggestionsAlertFactory
      alertCoordinatorForSuggestionItem:nil
                       onViewController:viewController
                                atPoint:CGPointMake(50, 50)
                            atIndexPath:nil
                         commandHandler:nil];
  [coordinator start];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_APP_CANCEL)]
      assertWithMatcher:grey_interactable()];

  [coordinator stop];
}

@end
