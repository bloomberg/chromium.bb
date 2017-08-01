// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/test/scoped_command_line.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_learn_more_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Select the cell with the |matcher| by scrolling the collection.
// 150 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* CellWithMatcher(id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_sufficientlyVisible(),
                                          nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_accessibilityID(
                               [ContentSuggestionsViewController
                                   collectionAccessibilityIdentifier])];
}
}

// Test case for the ContentSuggestion UI.
@interface ContentSuggestionsTestCase : ChromeTestCase {
  base::test::ScopedCommandLine _scopedCommandLine;
}
@end

@implementation ContentSuggestionsTestCase

+ (void)setUp {
  [super setUp];
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState, ntp_snippets::CreateChromeContentSuggestionsService);

  ntp_snippets::RegisterReadingListProvider(
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState),
      browserState);
}

+ (void)tearDown {
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Resets the Service associated with this browserState to a service with
  // default providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      ntp_snippets::CreateChromeContentSuggestionsServiceWithProviders);
  [super tearDown];
}

- (void)setUp {
  // The command line is set up before [super setUp] in order to have the NTP
  // opened with the command line already setup.
  base::CommandLine* commandLine = _scopedCommandLine.GetProcessCommandLine();
  commandLine->AppendSwitch(switches::kEnableSuggestionsUI);
  [super setUp];
}

// Tests that the "Learn More" cell is present.
- (void)testLearnMore {
  [CellWithMatcher(grey_accessibilityID(
      [ContentSuggestionsLearnMoreItem accessibilityIdentifier]))
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
