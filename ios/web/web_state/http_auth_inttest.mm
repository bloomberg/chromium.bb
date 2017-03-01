// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#import "ios/web/public/test/response_providers/http_auth_response_provider.h"
#import "ios/web/test/web_int_test.h"
#import "testing/gtest_mac.h"
#include "url/gurl.h"

namespace web {

using test::HttpServer;

// Test fixture for WebStateDelegate::OnAuthRequired integration tests.
class HttpAuthTest : public WebIntTest {
 protected:
  // Waits until WebStateDelegate::OnAuthRequired callback is called.
  void WaitForOnAuthRequiredCallback() {
    web_state_delegate_.ClearLastAuthenticationRequest();
    base::test::ios::WaitUntilCondition(^bool {
      return web_state_delegate_.last_authentication_request();
    });
  }

  // Loads a page with URL and waits for OnAuthRequired callback.
  void LoadUrlWithAuthChallenge(const GURL& url) {
    NavigationManager::WebLoadParams params(url);
    navigation_manager()->LoadURLWithParams(params);
    WaitForOnAuthRequiredCallback();
  }

  // Authenticates and waits until the page finishes loading.
  void Authenticate(NSString* username, NSString* password) {
    ASSERT_TRUE(web_state()->IsLoading());
    auto* auth_request = web_state_delegate_.last_authentication_request();
    auth_request->auth_callback.Run(username, password);
    base::test::ios::WaitUntilCondition(^bool {
      return !web_state()->IsLoading();
    });
  }
};

// Tests successful basic authentication.
TEST_F(HttpAuthTest, SuccessfullBasicAuth) {
  // Load the page which requests basic HTTP authentication.
  GURL url = HttpServer::MakeUrl("http://good-auth");
  test::SetUpHttpServer(base::MakeUnique<HttpAuthResponseProvider>(
      url, "GoodRealm", "gooduser", "goodpass"));
  LoadUrlWithAuthChallenge(url);

  // Verify that callback receives correct WebState.
  auto* auth_request = web_state_delegate_.last_authentication_request();
  EXPECT_EQ(web_state(), auth_request->web_state);

  // Verify that callback receives correctly configured protection space.
  NSURLProtectionSpace* protection_space = auth_request->protection_space;
  EXPECT_NSEQ(@"GoodRealm", protection_space.realm);
  EXPECT_FALSE(protection_space.receivesCredentialSecurely);
  EXPECT_FALSE([protection_space isProxy]);
  EXPECT_EQ(url.host(), base::SysNSStringToUTF8(protection_space.host));
  EXPECT_EQ(
      base::checked_cast<uint16_t>(HttpServer::GetSharedInstance().GetPort()),
      base::checked_cast<uint16_t>(protection_space.port));
  EXPECT_FALSE(protection_space.proxyType);
  EXPECT_NSEQ(NSURLProtectionSpaceHTTP, protection_space.protocol);
  EXPECT_NSEQ(NSURLAuthenticationMethodHTTPBasic,
              protection_space.authenticationMethod);

  // Make sure that authenticated page renders expected text.
  Authenticate(@"gooduser", @"goodpass");
  id document_body = ExecuteJavaScript(@"document.body.innerHTML");
  EXPECT_EQ(HttpAuthResponseProvider::page_text(),
            base::SysNSStringToUTF8(document_body));
}

// Tests unsuccessful basic authentication.
TEST_F(HttpAuthTest, UnsucessfulBasicAuth) {
  // Load the page which requests basic HTTP authentication.
  GURL url = HttpServer::MakeUrl("http://bad-auth");
  test::SetUpHttpServer(base::MakeUnique<HttpAuthResponseProvider>(
      url, "BadRealm", "baduser", "badpass"));
  LoadUrlWithAuthChallenge(url);

  // Make sure that incorrect credentials request authentication again.
  auto* auth_request = web_state_delegate_.last_authentication_request();
  auth_request->auth_callback.Run(@"gooduser", @"goodpass");
  WaitForOnAuthRequiredCallback();

  // Verify that callback receives correct WebState.
  auth_request = web_state_delegate_.last_authentication_request();
  EXPECT_EQ(web_state(), auth_request->web_state);

  // Verify that callback receives correctly configured protection space.
  NSURLProtectionSpace* protection_space = auth_request->protection_space;
  EXPECT_NSEQ(@"BadRealm", protection_space.realm);
  EXPECT_FALSE(protection_space.receivesCredentialSecurely);
  EXPECT_FALSE([protection_space isProxy]);
  EXPECT_EQ(url.host(), base::SysNSStringToUTF8(protection_space.host));
  EXPECT_EQ(
      base::checked_cast<uint16_t>(HttpServer::GetSharedInstance().GetPort()),
      base::checked_cast<uint16_t>(protection_space.port));
  EXPECT_FALSE(protection_space.proxyType);
  EXPECT_NSEQ(NSURLProtectionSpaceHTTP, protection_space.protocol);
  EXPECT_NSEQ(NSURLAuthenticationMethodHTTPBasic,
              protection_space.authenticationMethod);

  // Cancel authentication and make sure that the page is blank.
  auth_request->auth_callback.Run(nil, nil);
  base::test::ios::WaitUntilCondition(^bool {
    return !web_state()->IsLoading();
  });
  EXPECT_FALSE(ExecuteJavaScript(@"window.document"));
}

}  // web
