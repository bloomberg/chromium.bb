// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/html_response_provider.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

using testing::WaitUntilConditionOrTimeout;
using testing::kWaitForPageLoadTimeout;
using chrome_test_util::webViewContainingText;

namespace {

// Serves a page which requires Basic HTTP Authentication.
class HttpAuthResponseProvider : public HtmlResponseProvider {
 public:
  // Constructs provider which will respond to the given |url| and will use the
  // given authenticaion |realm|. |username| and |password| are credentials
  // required for sucessfull authentication. Use different realms and
  // username/password combination for different tests to prevent credentials
  // caching.
  HttpAuthResponseProvider(const GURL& url,
                           const std::string& realm,
                           const std::string& username,
                           const std::string& password)
      : url_(url), realm_(realm), username_(username), password_(password) {}
  ~HttpAuthResponseProvider() override {}

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override {
    return request.url == url_;
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    if (HeadersHaveValidCredentials(request.headers)) {
      *response_body = page_text();
      *headers = GetDefaultResponseHeaders();
    } else {
      *headers = GetResponseHeaders("", net::HTTP_UNAUTHORIZED);
      (*headers)->AddHeader(base::StringPrintf(
          "WWW-Authenticate: Basic realm=\"%s\"", realm_.c_str()));
    }
  }

  // Text returned in response if authentication was successfull.
  static std::string page_text() { return "authenticated"; }

 private:
  // Check if authorization header has valid credintials:
  // https://tools.ietf.org/html/rfc1945#section-10.2
  bool HeadersHaveValidCredentials(const net::HttpRequestHeaders& headers) {
    std::string header;
    if (headers.GetHeader(net::HttpRequestHeaders::kAuthorization, &header)) {
      std::string auth =
          base::StringPrintf("%s:%s", username_.c_str(), password_.c_str());
      std::string encoded_auth;
      base::Base64Encode(auth, &encoded_auth);
      return header == base::StringPrintf("Basic %s", encoded_auth.c_str());
    }
    return false;
  }

  // URL this provider responds to.
  GURL url_;
  // HTTP Authentication realm.
  std::string realm_;
  // Correct username to pass authentication
  std::string username_;
  // Correct password to pass authentication
  std::string password_;
};

// Returns matcher for HTTP Authentication dialog.
id<GREYMatcher> httpAuthDialog() {
  NSString* title = l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE);
  return chrome_test_util::staticTextWithAccessibilityLabel(title);
}

// Returns matcher for Username text field.
id<GREYMatcher> usernameField() {
  return chrome_test_util::staticTextWithAccessibilityLabelId(
      IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
}

// Returns matcher for Password text field.
id<GREYMatcher> passwordField() {
  return chrome_test_util::staticTextWithAccessibilityLabelId(
      IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
}

// Returns matcher for Login button.
id<GREYMatcher> loginButton() {
  return chrome_test_util::buttonWithAccessibilityLabelId(
      IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
}

// Waits until static text with IDS_LOGIN_DIALOG_TITLE label is displayed.
void WaitForHttpAuthDialog() {
  BOOL dialog_shown = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:httpAuthDialog()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return !error;
  });
  GREYAssert(dialog_shown, @"HTTP Authentication dialog was not shown");
}

}  // namespace

// Test case for HTTP Authentication flow.
@interface HTTPAuthTestCase : ChromeTestCase
@end

@implementation HTTPAuthTestCase

// Tests Basic HTTP Authentication with correct username and password.
- (void)testSuccessfullBasicAuth {
  if (IsIPadIdiom()) {
    // EG does not allow interactions with HTTP Dialog when loading spinner is
    // animated. TODO(crbug.com/680290): Enable this test on iPad when EarlGrey
    // allows tapping dialog buttons with active page load spinner.
    EARL_GREY_TEST_DISABLED(@"Tab Title not displayed on handset.");
  }

  GURL URL = web::test::HttpServer::MakeUrl("http://good-auth");
  web::test::SetUpHttpServer(base::MakeUnique<HttpAuthResponseProvider>(
      URL, "GoodRealm", "gooduser", "goodpass"));
  chrome_test_util::LoadUrl(URL);
  WaitForHttpAuthDialog();

  // Enter valid username and password.
  [[EarlGrey selectElementWithMatcher:usernameField()]
      performAction:grey_typeText(@"gooduser")];
  [[EarlGrey selectElementWithMatcher:passwordField()]
      performAction:grey_typeText(@"goodpass")];
  [[EarlGrey selectElementWithMatcher:loginButton()] performAction:grey_tap()];

  const std::string pageText = HttpAuthResponseProvider::page_text();
  [[EarlGrey selectElementWithMatcher:webViewContainingText(pageText)]
      assertWithMatcher:grey_notNil()];
}

// Tests Basic HTTP Authentication with incorrect username and password.
- (void)testUnsuccessfullBasicAuth {
  if (IsIPadIdiom()) {
    // EG does not allow interactions with HTTP Dialog when loading spinner is
    // animated. TODO(crbug.com/680290): Enable this test on iPad when EarlGrey
    // allows tapping dialog buttons with active page load spinner.
    EARL_GREY_TEST_DISABLED(@"Tab Title not displayed on handset.");
  }

  GURL URL = web::test::HttpServer::MakeUrl("http://bad-auth");
  web::test::SetUpHttpServer(base::MakeUnique<HttpAuthResponseProvider>(
      URL, "BadRealm", "baduser", "badpass"));
  chrome_test_util::LoadUrl(URL);
  WaitForHttpAuthDialog();

  // Enter invalid username and password.
  [[EarlGrey selectElementWithMatcher:usernameField()]
      performAction:grey_typeText(@"gooduser")];
  [[EarlGrey selectElementWithMatcher:passwordField()]
      performAction:grey_typeText(@"goodpass")];
  [[EarlGrey selectElementWithMatcher:loginButton()] performAction:grey_tap()];

  // Verifies that authentication was requested again.
  WaitForHttpAuthDialog();
}

// Tests Cancelling Basic HTTP Authentication.
- (void)testCancellingBasicAuth {
  if (IsIPadIdiom()) {
    // EG does not allow interactions with HTTP Dialog when loading spinner is
    // animated. TODO(crbug.com/680290): Enable this test on iPad when EarlGrey
    // allows tapping dialog buttons with active page load spinner.
    EARL_GREY_TEST_DISABLED(@"Tab Title not displayed on handset.");
  }

  GURL URL = web::test::HttpServer::MakeUrl("http://cancel-auth");
  web::test::SetUpHttpServer(base::MakeUnique<HttpAuthResponseProvider>(
      URL, "CancellingRealm", "", ""));
  chrome_test_util::LoadUrl(URL);
  WaitForHttpAuthDialog();

  [[EarlGrey selectElementWithMatcher:chrome_test_util::cancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:httpAuthDialog()]
      assertWithMatcher:grey_nil()];
}

@end
