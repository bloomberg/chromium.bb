// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/credential_manager.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/passwords/credential_manager_util.h"
#include "ios/chrome/browser/ssl/ios_security_state_tab_helper.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/test/web_test_with_web_state.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace credential_manager {

namespace {

// Test hostname for cert verification.
constexpr char kHostName[] = "www.example.com";
// HTTPS origin corresponding to kHostName.
constexpr char kHttpsWebOrigin[] = "https://www.example.com/";
// HTTP origin corresponding to kHostName.
constexpr char kHttpWebOrigin[] = "http://www.example.com/";
// HTTP origin representing localhost. It should be considered secure.
constexpr char kLocalhostOrigin[] = "http://localhost";
// Origin with data scheme. It should be considered insecure.
constexpr char kDataUriSchemeOrigin[] = "data://www.example.com";
// File origin.
constexpr char kFileOrigin[] = "file://example_file";

// SSL certificate to load for testing.
constexpr char kCertFileName[] = "ok_cert.pem";

}  // namespace

// TODO(crbug.com/435048): once JSCredentialManager is implemented and used
// from CredentialManager methods, mock it and write unit tests for
// CredentialManager methods SendGetResponse, SendStoreResponse and
// SendPreventSilentAccessResponse.
class CredentialManagerTest : public web::WebTestWithWebState {
 public:
  void SetUp() override {
    WebTestWithWebState::SetUp();

    // Used indirectly by WebStateContentIsSecureHtml function.
    IOSSecurityStateTabHelper::CreateForWebState(web_state());
  }

  // Updates SSLStatus on web_state()->GetNavigationManager()->GetVisibleItem()
  // with given |cert_status|, |security_style| and |content_status|.
  // SSLStatus fields |certificate|, |connection_status| and |cert_status_host|
  // are the same for all tests.
  void UpdateSslStatus(net::CertStatus cert_status,
                       web::SecurityStyle security_style,
                       web::SSLStatus::ContentStatusFlags content_status) {
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
    web::SSLStatus& ssl =
        web_state()->GetNavigationManager()->GetVisibleItem()->GetSSL();
    ssl.security_style = security_style;
    ssl.certificate = cert;
    ssl.cert_status = cert_status;
    ssl.connection_status = net::SSL_CONNECTION_VERSION_SSL3;
    ssl.content_status = content_status;
    ssl.cert_status_host = kHostName;
  }
};

class WebStateContentIsSecureHtmlTest : public CredentialManagerTest {};

// Tests that HTTPS websites with valid SSL certificate are recognized as
// secure.
TEST_F(WebStateContentIsSecureHtmlTest, AcceptHttpsUrls) {
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  UpdateSslStatus(net::CERT_STATUS_IS_EV, web::SECURITY_STYLE_AUTHENTICATED,
                  web::SSLStatus::NORMAL_CONTENT);
  EXPECT_TRUE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that WebStateContentIsSecureHtml returns false for HTTP origin.
TEST_F(WebStateContentIsSecureHtmlTest, HttpIsNotSecureContext) {
  LoadHtml(@"<html></html>", GURL(kHttpWebOrigin));
  EXPECT_FALSE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that WebStateContentIsSecureHtml returns false for HTTPS origin with
// valid SSL certificate but mixed contents.
TEST_F(WebStateContentIsSecureHtmlTest, InsecureContent) {
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  UpdateSslStatus(net::CERT_STATUS_IS_EV, web::SECURITY_STYLE_AUTHENTICATED,
                  web::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  EXPECT_FALSE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that WebStateContentIsSecureHtml returns false for HTTPS origin with
// invalid SSL certificate.
TEST_F(WebStateContentIsSecureHtmlTest, InvalidSslCertificate) {
  LoadHtml(@"<html></html>", GURL(kHttpsWebOrigin));
  UpdateSslStatus(net::CERT_STATUS_INVALID, web::SECURITY_STYLE_UNAUTHENTICATED,
                  web::SSLStatus::NORMAL_CONTENT);
  EXPECT_FALSE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that data:// URI scheme is not accepted as secure context.
TEST_F(WebStateContentIsSecureHtmlTest, DataUriSchemeIsNotSecureContext) {
  LoadHtml(@"<html></html>", GURL(kDataUriSchemeOrigin));
  EXPECT_FALSE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that localhost is accepted as secure context.
TEST_F(WebStateContentIsSecureHtmlTest, LocalhostIsSecureContext) {
  LoadHtml(@"<html></html>", GURL(kLocalhostOrigin));
  EXPECT_TRUE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that file origin is accepted as secure context.
TEST_F(WebStateContentIsSecureHtmlTest, FileIsSecureContext) {
  LoadHtml(@"<html></html>", GURL(kFileOrigin));
  EXPECT_TRUE(WebStateContentIsSecureHtml(web_state()));
}

// Tests that content must be HTML.
TEST_F(WebStateContentIsSecureHtmlTest, ContentMustBeHtml) {
  // No HTML is loaded on purpose, so that web_state()->ContentIsHTML() will
  // return false.
  EXPECT_FALSE(WebStateContentIsSecureHtml(web_state()));
}

}  // namespace credential_manager
