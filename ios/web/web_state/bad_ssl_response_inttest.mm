// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/public/certificate_policy_cache.h"
#import "ios/web/public/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/features.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#include "ios/web/public/web_state/session_certificate_policy_cache.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/default_handlers.h"
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
    RegisterDefaultHandlers(&https_server_);
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
  web_client()->SetAllowCertificateErrors(false);
  const GURL url = https_server_.GetURL("/");
  test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return !web_state()->IsLoading();
  }));

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

// Tests navigation to a page with self signed SSL cert and allowing the load
// via WebClient.
TEST_P(BadSslResponseTest, AllowLoad) {
  web_client()->SetAllowCertificateErrors(true);
  GURL url(https_server_.GetURL("/echo"));
  test::LoadUrl(web_state(), url);

  // Wait for ssl error.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return web_client()->last_cert_error_ssl_info().is_valid();
  }));

  // Verify SSL error.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            web_client()->last_cert_error_code());
  EXPECT_EQ(url, web_client()->last_cert_error_request_url());
  EXPECT_TRUE(web_client()->last_cert_error_overridable());

  const net::SSLInfo& ssl_info = web_client()->last_cert_error_ssl_info();
  EXPECT_TRUE(ssl_info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_AUTHORITY_INVALID, ssl_info.cert_status);
  ASSERT_TRUE(ssl_info.cert);
  EXPECT_EQ(url.host(), ssl_info.cert->subject().GetDisplayName());

  // WebClient::AllowCertificateError was set to return true. Now wait until
  // the load succeeds.
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());

  // Verify that |UpdateCertificatePolicyCache| correctly updates
  // CertificatePolicyCache from another BrowserState.
  TestBrowserState other_browser_state;
  auto cache = BrowserState::GetCertificatePolicyCache(&other_browser_state);
  scoped_refptr<net::X509Certificate> cert = https_server_.GetCertificate();
  CertPolicy::Judgment default_judgement = cache->QueryPolicy(
      cert.get(), url.host(), net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_EQ(CertPolicy::Judgment::UNKNOWN, default_judgement);
  web_state()->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
      cache);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    CertPolicy::Judgment policy = cache->QueryPolicy(
        cert.get(), url.host(), net::CERT_STATUS_AUTHORITY_INVALID);
    return CertPolicy::Judgment::ALLOWED == policy;
  }));

  // Verify that |BuildSessionStorage| correctly saves certificate decision
  // to CRWSessionCertificateStorage.
  CRWSessionCertificatePolicyCacheStorage* cache_storage =
      web_state()->BuildSessionStorage().certPolicyCacheStorage;
  ASSERT_TRUE(cache_storage);
  ASSERT_EQ(1U, cache_storage.certificateStorages.count);
  CRWSessionCertificateStorage* cert_storage =
      [cache_storage.certificateStorages anyObject];
  EXPECT_TRUE(cert->EqualsIncludingChain(cert_storage.certificate));
  EXPECT_EQ(url.host(), cert_storage.host);
  EXPECT_EQ(net::CERT_STATUS_AUTHORITY_INVALID, cert_storage.status);
}

// Tests creating WebState with CRWSessionCertificateStorage and populating
// CertificatePolicyCache.
TEST_P(BadSslResponseTest, ReadFromSessionCertificateStorage) {
  // Create WebState with CRWSessionCertificateStorage.
  GURL url(https_server_.GetURL("/echo"));
  scoped_refptr<net::X509Certificate> cert = https_server_.GetCertificate();

  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.lastCommittedItemIndex = -1;
  session_storage.certPolicyCacheStorage =
      [[CRWSessionCertificatePolicyCacheStorage alloc] init];
  CRWSessionCertificateStorage* cert_storage =
      [[CRWSessionCertificateStorage alloc]
          initWithCertificate:cert
                         host:url.host()
                       status:net::CERT_STATUS_AUTHORITY_INVALID];
  session_storage.certPolicyCacheStorage.certificateStorages =
      [NSSet setWithObject:cert_storage];

  WebState::CreateParams params(GetBrowserState());
  std::unique_ptr<WebState> web_state =
      WebState::CreateWithStorageSession(params, session_storage);
  web_state->GetNavigationManager()->LoadIfNecessary();

  // CertificatePolicyCache must be updated manually.
  web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
      BrowserState::GetCertificatePolicyCache(GetBrowserState()));

  // Navigation should succeed without consulting AllowCertificateErrors.
  web_client()->SetAllowCertificateErrors(false);
  test::LoadUrl(web_state.get(), url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state.get(), "Echo"));
  EXPECT_EQ(url, web_state->GetLastCommittedURL());
}

// Tests navigation to a page with self signed SSL cert and allowing the load
// via WebClient. Subsequent navigation should not call AllowCertificateError
// but always allow the load.
TEST_P(BadSslResponseTest, RememberCertDecision) {
  // Allow the load via WebClient.
  web_client()->SetAllowCertificateErrors(true);
  GURL url(https_server_.GetURL("/echo"));
  test::LoadUrl(web_state(), url);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());

  // Make sure that second loads succeeds without consulting WebClient.
  web_client()->SetAllowCertificateErrors(false);
  GURL url2(https_server_.GetURL("/echoall"));
  test::LoadUrl(web_state(), url2);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "GET /echoall"));
  EXPECT_EQ(url2, web_state()->GetLastCommittedURL());
}

INSTANTIATE_TEST_SUITE_P(
    ProgrammaticBadSslResponseTest,
    BadSslResponseTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

}  // namespace web {
