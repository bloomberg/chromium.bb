// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#include "ios/web/public/test/http_server/error_page_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::OmniboxText;
using chrome_test_util::TapWebViewElementWithId;
using web::test::HttpServer;

// Tests display of error pages for bad URLs.
@interface ExternalURLErrorPageTestCase : ChromeTestCase

// Checks that the DNS error page is not visible.
- (void)checkErrorPageIsNotVisible;

@end

@implementation ExternalURLErrorPageTestCase

#pragma mark - utilities

- (void)checkErrorPageIsNotVisible {
  // Check that the webview belongs to the web controller, and that the error
  // text doesn't appear in the webview.
  id<GREYMatcher> webViewMatcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:webViewMatcher]
      assertWithMatcher:grey_notNil()];
  const std::string kError =
      l10n_util::GetStringUTF8(IDS_ERRORPAGES_HEADING_NOT_AVAILABLE);
  [ChromeEarlGrey waitForWebViewNotContainingText:kError];
}

#pragma mark - tests

// Tests whether the error page is displayed for a bad URL.
- (void)testErrorPage {
// TODO(crbug.com/694662): This test relies on external URL because of the bug.
// Re-enable this test on device once the bug is fixed.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif

  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:ErrorPageResponseProvider::GetDnsFailureUrl()];

  [ChromeEarlGrey waitForErrorPage];
}

// Tests whether the error page is displayed if it is behind a redirect.
- (void)testErrorPageRedirect {
// TODO(crbug.com/694662): This test relies on external URL because of the bug.
// Re-enable this test on device once the bug is fixed.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif

  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  // Load a URL that redirects to the DNS-failing URL.
  [ChromeEarlGrey
      loadURL:ErrorPageResponseProvider::GetRedirectToDnsFailureUrl()];

  // Verify that the redirect occurred before checking for the DNS error.
  const std::string& redirectedURL =
      ErrorPageResponseProvider::GetDnsFailureUrl().GetContent();
  [[EarlGrey selectElementWithMatcher:OmniboxText(redirectedURL)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey waitForErrorPage];
}

// Tests that the error page is not displayed if the bad URL is in a <iframe>
// tag.
- (void)testErrorPageInIFrame {
  std::map<GURL, std::string> responses;
  const GURL URL = HttpServer::MakeUrl("http://browsingErrorPageInIFrame");
  // This page contains an iframe to a bad URL.
  responses[URL] = std::string("This page contains an iframe.<iframe src='") +
                   ErrorPageResponseProvider::GetDnsFailureUrl().spec() +
                   "'>whatever</iframe>";
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:URL];

  [self checkErrorPageIsNotVisible];
}

// Tests that the error page is not displayed if the bad URL is in a <iframe>
// tag that is loaded after the initial page load completes.
- (void)testErrorPageInIFrameAfterDelay {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      HttpServer::MakeUrl("http://browsingErrorPageInIFrameAfterDelay");
  // This page adds an iframe to a bad URL one second after loading. When the
  // timer completes, some text is also added to the page so EarlGrey can detect
  // that the timer has completed.
  const std::string kTimerCompleted = "Timer completed";
  responses[URL] =
      std::string("This page will have an iframe appended after page load.") +
      "<script>setTimeout(" + "function() { document.body.innerHTML+='<p>" +
      kTimerCompleted + "</p>" + "<iframe src=\"" +
      ErrorPageResponseProvider::GetDnsFailureUrl().spec() +
      "\"></iframe>'}, 1000);" + "</script>";
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:URL];
  // Check that the timer has completed.
  [ChromeEarlGrey waitForWebViewContainingText:kTimerCompleted];
  // DNS error page should still not appear.
  [self checkErrorPageIsNotVisible];
}

// Tests that the error page is not displayed if the navigation was not
// user-initiated.
- (void)testErrorPageNoUserInteraction {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      HttpServer::MakeUrl("http://browsingErrorPageNoUserInteraction");
  // This page contains a button that starts a timer that appends an iframe to a
  // bad URL. To create a non-user-initiated navigation, the timer takes longer
  // than the CRWWebController's kMaximumDelayForUserInteractionInSeconds
  // constant. When the timer completes, some text is also added to the page so
  // the test can detect that the timer has completed.
  const std::string kTimerCompleted = "Timer completed";
  const std::string kButtonId = "aButton";
  // Timeout used for the setTimeout() call the button invokes. This value must
  // be greater than CRWWebController's kMaximumDelayForUserInteractionInSeconds
  // and less than testing::kWaitForUIElementTimeout
  const std::string kTimeoutMs = "3000";
  responses[URL] = std::string("<form><input type='button' id='") + kButtonId +
                   "' value='button' onClick='setTimeout(function() { " +
                   "document.body.innerHTML+=\"<p>" + kTimerCompleted +
                   "</p><iframe src=" +
                   ErrorPageResponseProvider::GetDnsFailureUrl().spec() +
                   ">\"}, " + kTimeoutMs + ");'></form>";
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:URL];
  TapWebViewElementWithId(kButtonId);
  // Check that the timer has completed.
  [ChromeEarlGrey waitForWebViewContainingText:kTimerCompleted];
  // DNS error page should still not appear.
  [self checkErrorPageIsNotVisible];
}

@end
