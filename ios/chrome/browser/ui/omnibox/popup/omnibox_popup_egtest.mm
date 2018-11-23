// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the popup row containing the |url| as suggestion.
id<GREYMatcher> PopupRowWithUrl(GURL url) {
  return grey_allOf(
      grey_kindOfClass([OmniboxPopupRow class]),
      grey_descendant(chrome_test_util::StaticTextWithAccessibilityLabel(
          base::SysUTF8ToNSString(url.GetContent()))),
      nil);
}

// Web page 1.
const char kPage1[] = "This is the first page";
const char kPage1Title[] = "Title 1";
const char kPage1URL[] = "/page1.html";

// Web page 2.
const char kPage2[] = "This is the second page";
const char kPage2Title[] = "Title 2";
const char kPage2URL[] = "/page2.html";

// Web page 2.
const char kPage3[] = "This is the third page";
const char kPage3Title[] = "Title 3";
const char kPage3URL[] = "/page3.html";

// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPage1URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage1Title) +
        "</title></head><body>" + std::string(kPage1) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage2URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage2Title) +
        "</title></head><body>" + std::string(kPage2) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage3URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage3Title) +
        "</title></head><body>" + std::string(kPage3) + "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

}  //  namespace

@interface OmniboxPopupTestCase : ChromeTestCase {
  base::test::ScopedFeatureList _featureList;
}

@end

@implementation OmniboxPopupTestCase

- (void)setUp {
  [super setUp];
  // Enable the Switch to Open Tab flag.
  _featureList.InitAndEnableFeature(omnibox::kOmniboxTabSwitchSuggestions);

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that tapping the switch to open tab button, switch to the open tab,
// doesn't close the tab.
- (void)testSwitchToOpenTab {
  // Open the first page.
  GURL firstPageURL = self.testServer->GetURL(kPage1URL);
  [ChromeEarlGrey loadURL:firstPageURL];
  [ChromeEarlGrey waitForWebViewContainingText:kPage1];

  // Open the second page in another tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
  [ChromeEarlGrey waitForWebViewContainingText:kPage2];

  // Type the URL of the first page in the omnibox to trigger it as suggestion.
  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(kPage1URL)];

  // Switch to the first tab.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(firstPageURL)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebViewContainingText:kPage1];

  // Check that both tabs are opened (and that we switched tab and not just
  // navigated.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridOpenButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(kPage2Title)),
                     grey_ancestor(chrome_test_util::TabGridCellAtIndex(1)),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the switch to open tab button isn't displayed for the current tab.
- (void)testNotSwitchButtonOnCurrentTab {
  GURL URL2 = self.testServer->GetURL(kPage2URL);

  // Open the first page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1URL)];
  [ChromeEarlGrey waitForWebViewContainingText:kPage1];

  // Open the second page in another tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebViewContainingText:kPage2];

  // Type the URL of the first page in the omnibox to trigger it as suggestion.
  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(kPage2URL)];

  // Check that we have the suggestion for the second page, but not the switch
  // as it is the current page.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(URL2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL2)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];
}

// Tests that the incognito tabs aren't displayed as "opened" tab in the
// non-incognito suggestions and vice-versa.
- (void)testIncognitoSeparation {
  GURL URL1 = self.testServer->GetURL(kPage1URL);
  GURL URL2 = self.testServer->GetURL(kPage2URL);
  GURL URL3 = self.testServer->GetURL(kPage3URL);

  // Add all the pages to the history.
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebViewContainingText:kPage1];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebViewContainingText:kPage2];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey waitForWebViewContainingText:kPage3];
  [[self class] closeAllTabs];

  // Load page 1 in non-incognito and page 2 in incognito.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL1];
  [ChromeEarlGrey waitForWebViewContainingText:kPage1];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL2];
  [ChromeEarlGrey waitForWebViewContainingText:kPage2];

  // Open page 3 in non-incognito.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey waitForWebViewContainingText:kPage3];

  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(URL3.host())];

  // Check that we have the switch button for the first page.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL1)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that we have the suggestion for the second page, but not the switch.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(URL2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL2)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];

  // Open page 3 in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL3];
  [ChromeEarlGrey waitForWebViewContainingText:kPage3];

  [ChromeEarlGreyUI focusOmniboxAndType:base::SysUTF8ToNSString(URL3.host())];

  // Check that we have the switch button for the second page.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL2)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that we have the suggestion for the first page, but not the switch.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(URL1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(URL1)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_nil()];
}

@end
