// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_model.h"

#include "chrome/browser/ssl/security_state_model_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/cert_store.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kUrl[] = "https://foo.test";

class TestSecurityStateModelClient : public SecurityStateModelClient {
 public:
  TestSecurityStateModelClient()
      : initial_security_style_(content::SECURITY_STYLE_AUTHENTICATED),
        connection_status_(net::SSL_CONNECTION_VERSION_TLS1_2
                           << net::SSL_CONNECTION_VERSION_SHIFT),
        cert_status_(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT),
        displayed_mixed_content_(false),
        ran_mixed_content_(false) {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "sha1_2016.pem");
  }
  ~TestSecurityStateModelClient() override {}

  void set_connection_status(int connection_status) {
    connection_status_ = connection_status;
  }
  void SetCipherSuite(uint16 ciphersuite) {
    net::SSLConnectionStatusSetCipherSuite(ciphersuite, &connection_status_);
  }
  void AddCertStatus(net::CertStatus cert_status) {
    cert_status_ |= cert_status;
  }
  void SetDisplayedMixedContent(bool displayed_mixed_content) {
    displayed_mixed_content_ = displayed_mixed_content;
  }
  void SetRanMixedContent(bool ran_mixed_content) {
    ran_mixed_content_ = ran_mixed_content;
  }
  void set_initial_security_style(content::SecurityStyle security_style) {
    initial_security_style_ = security_style;
  }

  // SecurityStateModelClient:
  void GetVisibleSecurityState(
      SecurityStateModel::VisibleSecurityState* state) override {
    state->url = GURL(kUrl);
    state->initial_security_style = initial_security_style_;
    state->cert_id = 1;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->security_bits = 256;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->ran_mixed_content = ran_mixed_content_;
  }

  bool RetrieveCert(scoped_refptr<net::X509Certificate>* cert) override {
    *cert = cert_;
    return true;
  }

  bool UsedPolicyInstalledCertificate() override { return false; }

  bool IsOriginSecure(const GURL& url) override {
    return content::IsOriginSecure(url);
  }

 private:
  content::SecurityStyle initial_security_style_;
  scoped_refptr<net::X509Certificate> cert_;
  int connection_status_;
  net::CertStatus cert_status_;
  bool displayed_mixed_content_;
  bool ran_mixed_content_;
};

class SecurityStateModelTest : public ChromeRenderViewHostTestHarness {};

// Tests that SHA1-signed certificates expiring in 2016 downgrade the
// security state of the page.
TEST_F(SecurityStateModelTest, SHA1Warning) {
  SecurityStateModel::SecurityInfo security_info;
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(client.RetrieveCert(&cert));
  SecurityStateModel::VisibleSecurityState visible_security_state;
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of mixed
// content.
TEST_F(SecurityStateModelTest, SHA1WarningMixedContent) {
  SecurityStateModel::SecurityInfo security_info;
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(client.RetrieveCert(&cert));
  client.SetDisplayedMixedContent(true);
  SecurityStateModel::VisibleSecurityState visible_security_state;
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::DISPLAYED_MIXED_CONTENT,
            security_info.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);

  client.set_initial_security_style(
      content::SECURITY_STYLE_AUTHENTICATION_BROKEN);
  client.SetDisplayedMixedContent(false);
  client.SetRanMixedContent(true);
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::RAN_MIXED_CONTENT,
            security_info.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::SECURITY_ERROR, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of major
// cert errors.
TEST_F(SecurityStateModelTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(client.RetrieveCert(&cert));
  SecurityStateModel::SecurityInfo security_info;
  client.set_initial_security_style(
      content::SECURITY_STYLE_AUTHENTICATION_BROKEN);
  client.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  SecurityStateModel::VisibleSecurityState visible_security_state;
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::SECURITY_ERROR, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST_F(SecurityStateModelTest, SecureProtocolAndCiphersuite) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(client.RetrieveCert(&cert));
  SecurityStateModel::SecurityInfo security_info;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16 ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  SecurityStateModel::VisibleSecurityState visible_security_state;
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_TRUE(security_info.is_secure_protocol_and_ciphersuite);
}

TEST_F(SecurityStateModelTest, NonsecureProtocol) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(client.RetrieveCert(&cert));
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16 ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_1
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  SecurityStateModel::SecurityInfo security_info;
  SecurityStateModel::VisibleSecurityState visible_security_state;
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_FALSE(security_info.is_secure_protocol_and_ciphersuite);
}

TEST_F(SecurityStateModelTest, NonsecureCiphersuite) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  scoped_refptr<net::X509Certificate> cert;
  ASSERT_TRUE(client.RetrieveCert(&cert));
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16 ciphersuite = 0xc0a0;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  SecurityStateModel::SecurityInfo security_info;
  client.SetCipherSuite(ciphersuite);
  SecurityStateModel::VisibleSecurityState visible_security_state;
  client.GetVisibleSecurityState(&visible_security_state);
  SecurityStateModel::SecurityInfoForRequest(&client, visible_security_state,
                                             cert, &security_info);
  EXPECT_FALSE(security_info.is_secure_protocol_and_ciphersuite);
}

}  // namespace
