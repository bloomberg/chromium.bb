// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/public/features.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// BadSslResponseTest is parameterized on this enum to test both
// LegacyNavigationManagerImpl and WKBasedNavigationManagerImpl.
enum NavigationManagerChoice {
  TEST_LEGACY_NAVIGATION_MANAGER,
  TEST_WK_BASED_NAVIGATION_MANAGER,
};

// Test fixture for loading https pages with self signed certificate.
class BadSslResponseTest
    : public WebTestWithWebState,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  BadSslResponseTest()
      : WebTestWithWebState(std::make_unique<TestWebClient>()),
        https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSlimNavigationManager);
    }
  }

  void SetUp() override {
    WebTestWithWebState::SetUp();
    ASSERT_TRUE(https_server_.Start());
  }

  TestWebClient* web_client() {
    return static_cast<TestWebClient*>(GetWebClient());
  }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(BadSslResponseTest);
};

// Tests navigation to a page with self signed SSL cert and rejecting the load
// via WebClient. Test verifies the arguments passed to
// WebClient::AllowCertificateError.
TEST_P(BadSslResponseTest, RejectLoad) {
  const GURL url = https_server_.GetURL("/");
  test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !web_state()->IsLoading();
  }));

  // By default TestWebClient rejects the load.

  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            web_client()->last_cert_error_code());
  EXPECT_EQ(url, web_client()->last_cert_error_request_url());
  EXPECT_TRUE(web_client()->last_cert_error_overridable());

  const net::SSLInfo& ssl_info = web_client()->last_cert_error_ssl_info();
  EXPECT_TRUE(ssl_info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);
  ASSERT_TRUE(ssl_info.cert);
  EXPECT_EQ(url.host(), ssl_info.cert->subject().GetDisplayName());
}

INSTANTIATE_TEST_CASE_P(
    ProgrammaticBadSslResponseTest,
    BadSslResponseTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

}  // namespace web {
