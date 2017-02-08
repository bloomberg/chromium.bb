// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/memory/ptr_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/error_page_response_provider.h"
#include "ios/web/public/test/response_providers/response_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Assert the activity service is visible by checking the "copy" button.
void AssertActivityServiceVisible() {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   @"Copy")]
      assertWithMatcher:grey_interactable()];
}

// Assert the activity service is not visible by checking the "copy" button.
void AssertActivityServiceNotVisible() {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Copy"),
                     grey_interactable(), nil)] assertWithMatcher:grey_nil()];
}

// Returns a button with a print label.
id<GREYMatcher> PrintButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(@"Print");
}

}  // namespace

// Earl grey integration tests for Activity Service Controller.
@interface ActivityServiceControllerTestCase : ChromeTestCase
@end

@implementation ActivityServiceControllerTestCase

// Test that when trying to print a page redirected to an unprintable page, a
// snackbar explaining that the page cannot be printed is displayed.
- (void)testActivityServiceControllerPrintAfterRedirectionToUnprintablePage {
  std::map<GURL, std::string> responses;
  const GURL regularPageURL = web::test::HttpServer::MakeUrl("http://choux");
  responses[regularPageURL] = "fleur";
  web::test::SetUpHttpServer(
      base::MakeUnique<ErrorPageResponseProvider>(responses));

  // Open a regular page and verify that you can share.
  [ChromeEarlGrey loadURL:regularPageURL];
  [ChromeEarlGreyUI openShareMenu];
  AssertActivityServiceVisible();

  // Open an error page.
  [ChromeEarlGrey loadURL:ErrorPageResponseProvider::GetDnsFailureUrl()];
  NSString* const kError =
      l10n_util::GetNSString(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::StaticHtmlViewContainingText(
                                   kError)] assertWithMatcher:grey_notNil()];

  // Execute the Print action.
  [[EarlGrey selectElementWithMatcher:PrintButton()] performAction:grey_tap()];

  // Verify that a toast notification appears.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"This page cannot be printed.")]
      assertWithMatcher:grey_interactable()];

  // Dismiss the snackbar.
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:
                          kBrowserViewControllerSnackbarCategory];
}

- (void)testActivityServiceControllerCantPrintUnprintablePages {
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  // Open a page with an error.
  [ChromeEarlGrey loadURL:ErrorPageResponseProvider::GetDnsFailureUrl()];

  // Verify that you can share, but that the Print action is not available.
  [ChromeEarlGreyUI openShareMenu];
  AssertActivityServiceVisible();
  [[EarlGrey selectElementWithMatcher:PrintButton()]
      assertWithMatcher:grey_nil()];
}

- (void)testActivityServiceControllerIsDisabled {
  // Open an un-shareable page.
  GURL kURL("chrome://version");
  [ChromeEarlGrey loadURL:kURL];
  // Verify that the share button is disabled.
  if (IsCompact()) {
    [ChromeEarlGreyUI openToolsMenu];
  }
  id<GREYMatcher> share_button = chrome_test_util::ShareButton();
  [[EarlGrey selectElementWithMatcher:share_button]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

- (void)testOpenActivityServiceControllerAndCopy {
  // Set up mock http server.
  std::map<GURL, std::string> responses;
  GURL url = web::test::HttpServer::MakeUrl("http://potato");
  responses[url] = "tomato";
  web::test::SetUpSimpleHttpServer(responses);

  // Open page and open the share menu.
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGreyUI openShareMenu];

  // Verify that the share menu is up and contains a Print action.
  AssertActivityServiceVisible();
  [[EarlGrey selectElementWithMatcher:PrintButton()]
      assertWithMatcher:grey_interactable()];

  // Start the Copy action and verify that the share menu gets dismissed.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   @"Copy")] performAction:grey_tap()];
  AssertActivityServiceNotVisible();
}

@end
