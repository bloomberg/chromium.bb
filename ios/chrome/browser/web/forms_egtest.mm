// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"
#include "ios/web/public/test/response_providers/html_response_provider_impl.h"

namespace {

// URL for a generic website in the user navigation flow.
const char kGenericUrl[] = "http://generic";

// URL for the server to print the HTTP method and the request body.
const char kPrintFormDataUrl[] = "http://printFormData";

// URL for the server that redirects to kPrintPostData with a 302.
const char kRedirectUrl[] = "http://redirect";

// URL for the server to return a page that posts a form with some data to
// |kPrintPostData|.
const char kFormUrl[] = "http://formURL";

// URL for the server to return a page that posts to |kRedirect|.
const char kRedirectFormUrl[] = "http://redirectFormURL";

// Label for the button in the form.
const char kSubmitButton[] = "Submit";

// Expected response from the server.
const char kExpectedPostData[] = "POST Data=Unicorn";

}  // namespace

// Tests submition of HTTP forms POST data including cases involving navigation.
@interface FormsTestCase : ChromeTestCase
@end

@implementation FormsTestCase

// Sets up server urls and responses.
- (void)setUp {
  [super setUp];
  std::map<GURL, HtmlResponseProviderImpl::Response> responses;

  const char* formHtml =
      "<form method=\"post\" action=\"%s\">"
      "<textarea rows=\"1\" name=\"Data\">Unicorn</textarea>"
      "<input type=\"submit\" value=\"%s\" id=\"%s\">"
      "</form>";
  GURL printFormDataUrl = web::test::HttpServer::MakeUrl(kPrintFormDataUrl);

  const GURL formUrl = web::test::HttpServer::MakeUrl(kFormUrl);
  responses[formUrl] = HtmlResponseProviderImpl::GetSimpleResponse(
      base::StringPrintf(formHtml, printFormDataUrl.spec().c_str(),
                         kSubmitButton, kSubmitButton));

  const GURL redirectFormUrl = web::test::HttpServer::MakeUrl(kRedirectFormUrl);
  const std::string redirectFormResponse = base::StringPrintf(
      formHtml, web::test::HttpServer::MakeUrl(kRedirectUrl).spec().c_str(),
      kSubmitButton, kSubmitButton);
  responses[redirectFormUrl] =
      HtmlResponseProviderImpl::GetSimpleResponse(redirectFormResponse);

  const GURL redirectUrl = web::test::HttpServer::MakeUrl(kRedirectUrl);
  responses[redirectUrl] = HtmlResponseProviderImpl::GetRedirectResponse(
      printFormDataUrl, net::HTTP_FOUND);

  std::unique_ptr<web::DataResponseProvider> provider(
      new HtmlResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));
}

// Submits the html form and verifies the destination url.
- (void)submitForm {
  chrome_test_util::TapWebViewElementWithId(kSubmitButton);

  GURL url = web::test::HttpServer::MakeUrl(kPrintFormDataUrl);
  id<GREYMatcher> URLMatcher = chrome_test_util::OmniboxText(url.GetContent());
  [[EarlGrey selectElementWithMatcher:URLMatcher]
      assertWithMatcher:grey_notNil()];
}

// Waits for the |expectedResponse| within the web view.
- (void)waitForExpectedResponse:(std::string)expectedResponse {
  [[GREYCondition
      conditionWithName:@"Waiting for webview to display resulting text."
                  block:^BOOL {
                    id<GREYMatcher> webViewMatcher =
                        chrome_test_util::WebViewContainingText(
                            expectedResponse);
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:webViewMatcher]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:5];
}

// Waits for view with Tab History accessibility ID.
- (void)waitForTabHistoryView {
  [[GREYCondition conditionWithName:@"Waiting for Tab History to display."
                              block:^BOOL {
                                NSError* error = nil;
                                id<GREYMatcher> tabHistory =
                                    grey_accessibilityID(@"Tab History");
                                [[EarlGrey selectElementWithMatcher:tabHistory]
                                    assertWithMatcher:grey_notNil()
                                                error:&error];
                                return error == nil;
                              }] waitWithTimeout:5];
}

// Reloads the web view and waits for the loading to complete.
// TODO(crbug.com/638674): Evaluate if this can move to shared code
- (void)reloadPage {
  base::scoped_nsobject<GenericChromeCommand> reloadCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_RELOAD]);
  chrome_test_util::RunCommandWithActiveViewController(reloadCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Navigates back to the previous webpage.
- (void)goBack {
  base::scoped_nsobject<GenericChromeCommand> backCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_BACK]);
  chrome_test_util::RunCommandWithActiveViewController(backCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Open back navigation history.
- (void)openBackHistory {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];
}

// Navigates forward to a previous webpage.
// TODO(crbug.com/638674): Evaluate if this can move to shared code
- (void)goForward {
  base::scoped_nsobject<GenericChromeCommand> forwardCommand(
      [[GenericChromeCommand alloc] initWithTag:IDC_FORWARD]);
  chrome_test_util::RunCommandWithActiveViewController(forwardCommand);

  [ChromeEarlGrey waitForPageToFinishLoading];
}

// Accepts the warning that the form POST data will be reposted.
- (void)confirmResendWarning {
  id<GREYMatcher> resendWarning =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_HTTP_POST_WARNING_RESEND);
  [[EarlGrey selectElementWithMatcher:resendWarning]
      performAction:grey_longPress()];
}

// Tests whether the request data is reposted correctly.
- (void)testRepostForm {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFormUrl)];

  [self submitForm];
  [self waitForExpectedResponse:kExpectedPostData];

  [self reloadPage];
  [self confirmResendWarning];

  [self waitForExpectedResponse:kExpectedPostData];
}

// Tests that a POST followed by navigating to a new page and then tapping back
// to the form result page resends data.
- (void)testRepostFormAfterTappingBack {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFormUrl)];

  [self submitForm];

  // Go to a new page.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kGenericUrl)];

  // Go back and check that the data is reposted.
  [self goBack];
  [self confirmResendWarning];
  [self waitForExpectedResponse:kExpectedPostData];
}

// Tests that a POST followed by tapping back to the form page and then tapping
// forward to the result page resends data.
- (void)testRepostFormAfterTappingBackAndForward {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFormUrl)];
  [self submitForm];

  [self goBack];
  [self goForward];
  [self confirmResendWarning];
  [self waitForExpectedResponse:kExpectedPostData];
}

// Tests that a POST followed by a new request and then index navigation to get
// back to the result page resends data.
- (void)testRepostFormAfterIndexNavigation {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFormUrl)];
  [self submitForm];

  // Go to a new page.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kGenericUrl)];

  [self openBackHistory];
  [self waitForTabHistoryView];

  const GURL printURL = web::test::HttpServer::MakeUrl(kPrintFormDataUrl);
  id<GREYMatcher> historyItem =
      grey_text(base::SysUTF8ToNSString(printURL.spec()));
  [[EarlGrey selectElementWithMatcher:historyItem] performAction:grey_tap()];

  [ChromeEarlGrey waitForPageToFinishLoading];

  [self confirmResendWarning];
  [self waitForExpectedResponse:kExpectedPostData];
}

// When data is not re-sent, the request is done with a GET method.
- (void)testRepostFormCancelling {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kFormUrl)];
  [self submitForm];

  [self reloadPage];

  // Abort the reload.
  if (IsIPadIdiom()) {
    // On tablet, dismiss the popover.
    base::scoped_nsobject<GREYElementMatcherBlock> matcher([
        [GREYElementMatcherBlock alloc]
        initWithMatchesBlock:^BOOL(UIView* view) {
          return [NSStringFromClass([view class]) hasPrefix:@"UIDimmingView"];
        }
        descriptionBlock:^(id<GREYDescription> description) {
          [description appendText:@"class prefixed with UIDimmingView"];
        }]);
    [[EarlGrey selectElementWithMatcher:matcher]
        performAction:grey_tapAtPoint(CGPointMake(50.0f, 50.0f))];
  } else {
    // On handset, dismiss via the cancel button.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
  // Check that the POST is changed to a GET
  [self waitForExpectedResponse:"GET"];
}

// Tests that a POST followed by a redirect does not show the popup.
- (void)testRepostFormCancellingAfterRedirect {
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kRedirectFormUrl)];
  // Submit the form, which redirects before printing the data.
  [self submitForm];
  // Check that the redirect changes the POST to a GET.
  [self waitForExpectedResponse:"GET"];
  [self reloadPage];

  // Check that the popup did not show
  id<GREYMatcher> resendWarning =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_HTTP_POST_WARNING_RESEND);
  [[EarlGrey selectElementWithMatcher:resendWarning]
      assertWithMatcher:grey_nil()];

  [self waitForExpectedResponse:"GET"];
}

@end
