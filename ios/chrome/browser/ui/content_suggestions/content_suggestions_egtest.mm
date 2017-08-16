// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include <vector>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/mock_content_suggestions_provider.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_learn_more_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace ntp_snippets;

namespace {

// Returns a suggestion created from the |category|, |suggestion_id| and the
// |url|.
ContentSuggestion Suggestion(Category category,
                             std::string suggestion_id,
                             GURL url) {
  ContentSuggestion suggestion(category, suggestion_id, url);
  suggestion.set_title(base::UTF8ToUTF16(url.spec()));

  return suggestion;
}

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

}  // namespace

#pragma mark - TestCase

// Test case for the ContentSuggestion UI.
@interface ContentSuggestionsTestCase : ChromeTestCase {
  base::test::ScopedCommandLine _scopedCommandLine;
}

// Current non-incognito browser state.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;
// Mock provider from the singleton.
@property(nonatomic, assign, readonly) MockContentSuggestionsProvider* provider;
// Article category, used by the singleton.
@property(nonatomic, assign, readonly) Category category;

@end

@implementation ContentSuggestionsTestCase

#pragma mark - Setup/Teardown

+ (void)setUp {
  [super setUp];
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState, CreateChromeContentSuggestionsService);

  ContentSuggestionsService* service =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState);
  RegisterReadingListProvider(service, browserState);
  [[ContentSuggestionsTestSingleton sharedInstance]
      registerArticleProvider:service];
}

+ (void)tearDown {
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  ReadingListModelFactory::GetForBrowserState(browserState)->DeleteAllEntries();

  // Resets the Service associated with this browserState to a service with
  // default providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState, CreateChromeContentSuggestionsServiceWithProviders);
  [super tearDown];
}

- (void)setUp {
  // The command line is set up before [super setUp] in order to have the NTP
  // opened with the command line already setup.
  base::CommandLine* commandLine = _scopedCommandLine.GetProcessCommandLine();
  commandLine->AppendSwitch(switches::kEnableSuggestionsUI);
  self.provider->FireCategoryStatusChanged(self.category,
                                           CategoryStatus::AVAILABLE);

  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  readingListModel->DeleteAllEntries();
  [super setUp];
}

- (void)tearDown {
  self.provider->FireCategoryStatusChanged(
      self.category, CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
  [super tearDown];
}

#pragma mark - Tests

// Tests that a switch for the ContentSuggestions exists in the settings. The
// behavior depends on having a real remote provider, so it cannot be tested
// here.
- (void)testPrivacySwitch {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPrivacyButton()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_OPTIONS_CONTENT_SUGGESTIONS)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the section titles are displayed only if there are two sections.
- (void)testSectionTitle {
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  readingListModel->AddEntry(GURL("http://chromium.org"), "test title",
                             reading_list::ADDED_VIA_CURRENT_APP);

  [CellWithMatcher(chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_HEADER))
      assertWithMatcher:grey_nil()];

  std::vector<ContentSuggestion> suggestions;
  suggestions.emplace_back(
      Suggestion(self.category, "chromium", GURL("http://chromium.org")));
  self.provider->FireSuggestionsChanged(self.category, std::move(suggestions));

  [CellWithMatcher(chrome_test_util::StaticTextWithAccessibilityLabelId(
      IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_HEADER))
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Learn More" cell is present only if there is a suggestion in
// the section.
- (void)testLearnMore {
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   [ContentSuggestionsViewController
                                       collectionAccessibilityIdentifier])]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          [ContentSuggestionsLearnMoreItem
                                              accessibilityIdentifier])]
      assertWithMatcher:grey_nil()];

  std::vector<ContentSuggestion> suggestions;
  suggestions.emplace_back(
      Suggestion(self.category, "chromium", GURL("http://chromium.org")));
  self.provider->FireSuggestionsChanged(self.category, std::move(suggestions));

  [CellWithMatcher(grey_accessibilityID(
      [ContentSuggestionsLearnMoreItem accessibilityIdentifier]))
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when long pressing a Reading List entry, a context menu is shown.
- (void)testReadingListLongPress {
  NSString* title = @"ReadingList test title";
  std::string sTitle = "ReadingList test title";
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);
  readingListModel->AddEntry(GURL("http://chromium.org"), sTitle,
                             reading_list::ADDED_VIA_CURRENT_APP);

  [CellWithMatcher(grey_accessibilityID(title)) performAction:grey_longPress()];

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
  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_APP_CANCEL)] assertWithMatcher:grey_interactable()];
  }

  // No read later as it is already in the Reading List section.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_nil()];
}

// Tests that when long pressing a Most Visited tile, a context menu is shown.
- (void)testMostVisitedLongPress {
  std::map<GURL, std::string> responses;
  GURL URL = web::test::HttpServer::MakeUrl("http://simple_tile.html");
  responses[URL] =
      "<head><title>title1</title></head>"
      "<body>You are here.</body>";
  web::test::SetUpSimpleHttpServer(responses);

  // Clear history and verify that the tile does not exist.
  chrome_test_util::ClearBrowsingHistory();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [ChromeEarlGrey loadURL:URL];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];

  chrome_test_util::OpenNewTab();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"title1")]
      performAction:grey_longPress()];

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
  if (!IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_APP_CANCEL)] assertWithMatcher:grey_interactable()];
  }

  chrome_test_util::ClearBrowsingHistory();
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

#pragma mark - Properties

- (ios::ChromeBrowserState*)browserState {
  return chrome_test_util::GetOriginalBrowserState();
}

- (MockContentSuggestionsProvider*)provider {
  return [[ContentSuggestionsTestSingleton sharedInstance] provider];
}

- (Category)category {
  return Category::FromKnownCategory(KnownCategories::ARTICLES);
}

@end
