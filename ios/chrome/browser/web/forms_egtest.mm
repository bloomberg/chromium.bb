// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
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
#include "ios/web/public/test/response_providers/data_response_provider.h"

namespace {

// URL for a generic website in the user navigation flow.
const char kGenericUrl[] = "http://generic";

// URL to print the HTTP method and the request body.
const char kPrintFormDataUrl[] = "http://printFormData";

// URL that redirects to kPrintPostData with a 302.
const char kRedirectUrl[] = "http://redirect";

// URL to return a page that posts a form with some data to
// |kPrintPostData|.
const char kFormUrl[] = "http://formURL";

// URL to return a page that posts to |kRedirect|.
const char kRedirectFormUrl[] = "http://redirectFormURL";

// Label for the button in the form.
const char kSubmitButton[] = "Submit";

// Expected response from the server.
const char kExpectedPostData[] = "POST Data=Unicorn";

#pragma mark - TestResponseProvider

// A ResponseProvider that provides html response or a redirect.
class TestResponseProvider : public web::DataResponseProvider {
 public:
  // URL for the server at |kGenericUrl|.
  static GURL GetGenericUrl() {
    return web::test::HttpServer::MakeUrl(kGenericUrl);
  }
  // URL for the server at |kPrintFormDataUrl|.
  static GURL GetPrintFormDataUrl() {
    return web::test::HttpServer::MakeUrl(kPrintFormDataUrl);
  }
  // URL for the server at |kRedirectUrl|.
  static GURL GetRedirectUrl() {
    return web::test::HttpServer::MakeUrl(kRedirectUrl);
  }
  // URL for the server at |kFormUrl|.
  static GURL GetFormUrl() { return web::test::HttpServer::MakeUrl(kFormUrl); }
  // URL for the server at |kRedirectFormUrl|.
  static GURL GetRedirectFormUrl() {
    return web::test::HttpServer::MakeUrl(kRedirectFormUrl);
  }
  // TestResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;
};

bool TestResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return url == TestResponseProvider::GetPrintFormDataUrl() ||
         url == TestResponseProvider::GetRedirectUrl() ||
         url == TestResponseProvider::GetFormUrl() ||
         url == TestResponseProvider::GetRedirectFormUrl();
}

void TestResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  if (url == TestResponseProvider::GetRedirectUrl()) {
    *headers = web::ResponseProvider::GetRedirectResponseHeaders(
        TestResponseProvider::GetPrintFormDataUrl().spec(), net::HTTP_FOUND);
    return;
  }

  const char* form_html =
      "<form method=\"post\" action=\"%s\">"
      "<textarea rows=\"1\" name=\"Data\">Unicorn</textarea>"
      "<input type=\"submit\" value=\"%s\" id=\"%s\">"
      "</form>";

  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url == TestResponseProvider::GetFormUrl()) {
    *response_body = base::StringPrintf(
        form_html, TestResponseProvider::GetPrintFormDataUrl().spec().c_str(),
        kSubmitButton, kSubmitButton);
    return;
  } else if (url == TestResponseProvider::GetRedirectFormUrl()) {
    *response_body = base::StringPrintf(
        form_html, TestResponseProvider::GetRedirectUrl().spec().c_str(),
        kSubmitButton, kSubmitButton);
    return;
  } else if (url == TestResponseProvider::GetPrintFormDataUrl()) {
    *response_body = request.method + std::string(" ") + request.body;
    return;
  }
  NOTREACHED();
}

}  // namespace

// Tests submition of HTTP forms POST data including cases involving navigation.
@interface FormsTestCase : ChromeTestCase
@end

@implementation FormsTestCase

// Sets up server urls and responses.
- (void)setUp {
  [super setUp];

  web::test::SetUpHttpServer(base::MakeUnique<TestResponseProvider>());
}

// Submits the html form and verifies the destination url.
- (void)submitForm {
  chrome_test_util::TapWebViewElementWithId(kSubmitButton);

  GURL url = TestResponseProvider::GetPrintFormDataUrl();
  id<GREYMatcher> URLMatcher = chrome_test_util::OmniboxText(url.GetContent());
  [[EarlGrey selectElementWithMatcher:URLMatcher]
      assertWithMatcher:grey_notNil()];
}

// Waits for the |expectedResponse| within the web view.
- (void)waitForExpectedResponse:(std::string)expectedResponse {
  GREYCondition* condition = [GREYCondition
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
                  }];
  GREYAssert([condition waitWithTimeout:5], @"Webview text was not displayed.");
}

// Waits for view with Tab History accessibility ID.
- (void)waitForTabHistoryView {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Waiting for Tab History to display."
                  block:^BOOL {
                    NSError* error = nil;
                    id<GREYMatcher> tabHistory =
                        grey_accessibilityID(@"Tab History");
                    [[EarlGrey selectElementWithMatcher:tabHistory]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssert([condition waitWithTimeout:5], @"Tab History View not displayed.");
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
  [ChromeEarlGrey loadURL:TestResponseProvider::GetFormUrl()];

  [self submitForm];
  [self waitForExpectedResponse:kExpectedPostData];

  [self reloadPage];
  [self confirmResendWarning];

  [self waitForExpectedResponse:kExpectedPostData];
}

// Tests that a POST followed by navigating to a new page and then tapping back
// to the form result page resends data.
- (void)testRepostFormAfterTappingBack {
  [ChromeEarlGrey loadURL:TestResponseProvider::GetFormUrl()];

  [self submitForm];

  // Go to a new page.
  [ChromeEarlGrey loadURL:TestResponseProvider::GetGenericUrl()];

  // Go back and check that the data is reposted.
  [self goBack];
  [self confirmResendWarning];
  [self waitForExpectedResponse:kExpectedPostData];
}

// Tests that a POST followed by tapping back to the form page and then tapping
// forward to the result page resends data.
- (void)testRepostFormAfterTappingBackAndForward {
  [ChromeEarlGrey loadURL:TestResponseProvider::GetFormUrl()];
  [self submitForm];

  [self goBack];
  [self goForward];
  [self confirmResendWarning];
  [self waitForExpectedResponse:kExpectedPostData];
}

// Tests that a POST followed by a new request and then index navigation to get
// back to the result page resends data.
- (void)testRepostFormAfterIndexNavigation {
  [ChromeEarlGrey loadURL:TestResponseProvider::GetFormUrl()];
  [self submitForm];

  // Go to a new page.
  [ChromeEarlGrey loadURL:TestResponseProvider::GetGenericUrl()];

  [self openBackHistory];
  [self waitForTabHistoryView];

  id<GREYMatcher> historyItem = grey_text(base::SysUTF8ToNSString(
      TestResponseProvider::GetPrintFormDataUrl().spec()));
  [[EarlGrey selectElementWithMatcher:historyItem] performAction:grey_tap()];

  [ChromeEarlGrey waitForPageToFinishLoading];

  [self confirmResendWarning];
  [self waitForExpectedResponse:kExpectedPostData];
}

// When data is not re-sent, the request is done with a GET method.
- (void)testRepostFormCancelling {
  [ChromeEarlGrey loadURL:TestResponseProvider::GetFormUrl()];
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
  [ChromeEarlGrey loadURL:TestResponseProvider::GetRedirectFormUrl()];
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
