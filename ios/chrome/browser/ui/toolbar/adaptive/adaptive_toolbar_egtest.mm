// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPageURL[] = "/test-page.html";
const char kPageURL2[] = "/test-page-2.html";
const char kPageURL3[] = "/test-page-3.html";
const char kLinkID[] = "linkID";
const char kPageLoadedString[] = "Page loaded!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<html><body><p>" + std::string(kPageLoadedString) + "</p><a href=\"" +
      kPageURL3 + "\" id=\"" + kLinkID + "\">link!</a></body></html>");
  return std::move(http_response);
}

id<GREYMatcher> BookmarkButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
}

id<GREYMatcher> ShareButton() {
  return grey_allOf(grey_accessibilityID(kToolbarShareButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> firstResponder() {
  MatchesBlock matches = ^BOOL(UIResponder* responder) {
    return [responder isFirstResponder];
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"first responder"];
  };
  return grey_allOf(
      grey_kindOfClass([UIResponder class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}
}

#pragma mark - TestCase

// Test case for the adaptive toolbar UI.
@interface AdaptiveToolbarTestCase : ChromeTestCase

@end

@implementation AdaptiveToolbarTestCase

// Tests that bookmarks button is selected for the bookmarked pages.
- (void)testBookmarkButton {
  // Setup the bookmarks.
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");

  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Navigate to a page and check the bookmark button is not selected.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:grey_not(grey_selected())];

  // Bookmark the page.
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:grey_selected()];

  // Navigate to a different page and check the button is not selected.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL2)];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:grey_not(grey_selected())];

  // Navigate back to the bookmarked page and check the button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:grey_selected()];

  // Clean the bookmarks
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tests that tapping a button cancels the focus on the omnibox.
- (void)testCancelOmniboxEdit {
  if (IsCompactWidth()) {
    EARL_GREY_TEST_SKIPPED(@"No button to tap in compact width.");
  }

  // Navigate to a page to enable the back button.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Focus the omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:firstResponder()];

  // Tap the back button and check the omnibox is unfocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_not(firstResponder())];
}

// Verifies that the back/forward buttons are working and are correctly enabled
// during navigations.
- (void)testNavigationButtons {
  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Loads two url and check the navigation buttons status.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL2)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Check the navigation to the second page occurred.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPageURL2)];

  // Go back.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPageURL)];

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_interactable()];

  // Go forward.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPageURL2)];

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Open a page in a new incognito tab to have the focus.
  [[EarlGrey
      selectElementWithMatcher:web::WebViewInWebState(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        kLinkID, true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that tapping the omnibox button focuses the omnibox.
- (void)testOmniboxButton {
  if (!IsSplitToolbarMode()) {
    EARL_GREY_TEST_SKIPPED(@"No omnibox button to tap.");
  }

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolbarOmniboxButtonIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:firstResponder()];
}

// Tests share button is enabled only on pages that can be shared.
- (void)testShareButton {
  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // The button is disabled on the NTP.
  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Navigate to another page and check that the share button is enabled.
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_interactable()];
}

// Verifies the existence and state of toolbar UI elements.
- (void)testToolbarUI {
  id<GREYMatcher> bookmarkButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);

  // Navigate to a page and verify the back button is enabled.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];

  // Check that the bookmark button is visible on compact width.
  if (IsCompactWidth()) {
    [[EarlGrey selectElementWithMatcher:bookmarkButton]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // TODO(crbug.com/801145): Test more pieces of UI.
}

@end
