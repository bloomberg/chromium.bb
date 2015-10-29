// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_model.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/cert_store.h"
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

void GetTestSSLStatus(int process_id, content::SSLStatus* ssl_status) {
  content::CertStore* cert_store = content::CertStore::GetInstance();
  const scoped_refptr<net::X509Certificate>& cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "sha1_2016.pem");
  ASSERT_TRUE(cert);
  ssl_status->cert_id = cert_store->StoreCert(cert.get(), process_id);
  EXPECT_GT(ssl_status->cert_id, 0);
  ssl_status->cert_status = net::CERT_STATUS_SHA1_SIGNATURE_PRESENT;
  ssl_status->security_bits = 256;
  ssl_status->connection_status = net::SSL_CONNECTION_VERSION_TLS1_2
                                  << net::SSL_CONNECTION_VERSION_SHIFT;
}

class SecurityStateModelTest : public ChromeRenderViewHostTestHarness {};

// Tests that SHA1-signed certificates expiring in 2016 downgrade the
// security state of the page.
TEST_F(SecurityStateModelTest, SHA1Warning) {
  GURL url(kUrl);
  Profile* test_profile = profile();
  SecurityStateModel::SecurityInfo security_info;
  content::SSLStatus ssl_status;
  ASSERT_NO_FATAL_FAILURE(GetTestSSLStatus(process()->GetID(), &ssl_status));
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of mixed
// content.
TEST_F(SecurityStateModelTest, SHA1WarningMixedContent) {
  GURL url(kUrl);
  Profile* test_profile = profile();
  SecurityStateModel::SecurityInfo security_info;
  content::SSLStatus ssl_status;
  ASSERT_NO_FATAL_FAILURE(GetTestSSLStatus(process()->GetID(), &ssl_status));
  ssl_status.content_status = content::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::DISPLAYED_MIXED_CONTENT,
            security_info.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);

  ssl_status.security_style = content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  ssl_status.content_status = content::SSLStatus::RAN_INSECURE_CONTENT;
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::RAN_MIXED_CONTENT,
            security_info.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::SECURITY_ERROR, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of major
// cert errors.
TEST_F(SecurityStateModelTest, SHA1WarningBrokenHTTPS) {
  GURL url(kUrl);
  Profile* test_profile = profile();
  SecurityStateModel::SecurityInfo security_info;
  content::SSLStatus ssl_status;
  ASSERT_NO_FATAL_FAILURE(GetTestSSLStatus(process()->GetID(), &ssl_status));
  ssl_status.security_style = content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  ssl_status.cert_status |= net::CERT_STATUS_DATE_INVALID;
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::SECURITY_ERROR, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST_F(SecurityStateModelTest, SecureProtocolAndCiphersuite) {
  GURL url(kUrl);
  Profile* test_profile = profile();
  SecurityStateModel::SecurityInfo security_info;
  content::SSLStatus ssl_status;
  ASSERT_NO_FATAL_FAILURE(GetTestSSLStatus(process()->GetID(), &ssl_status));
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16 ciphersuite = 0xc02f;
  ssl_status.connection_status =
      (net::SSL_CONNECTION_VERSION_TLS1_2 << net::SSL_CONNECTION_VERSION_SHIFT);
  net::SSLConnectionStatusSetCipherSuite(ciphersuite,
                                         &ssl_status.connection_status);
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_TRUE(security_info.is_secure_protocol_and_ciphersuite);
}

TEST_F(SecurityStateModelTest, NonsecureProtocol) {
  GURL url(kUrl);
  Profile* test_profile = profile();
  SecurityStateModel::SecurityInfo security_info;
  content::SSLStatus ssl_status;
  ASSERT_NO_FATAL_FAILURE(GetTestSSLStatus(process()->GetID(), &ssl_status));
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16 ciphersuite = 0xc02f;
  ssl_status.connection_status =
      (net::SSL_CONNECTION_VERSION_TLS1_1 << net::SSL_CONNECTION_VERSION_SHIFT);
  net::SSLConnectionStatusSetCipherSuite(ciphersuite,
                                         &ssl_status.connection_status);
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_FALSE(security_info.is_secure_protocol_and_ciphersuite);
}

TEST_F(SecurityStateModelTest, NonsecureCiphersuite) {
  GURL url(kUrl);
  Profile* test_profile = profile();
  SecurityStateModel::SecurityInfo security_info;
  content::SSLStatus ssl_status;
  ASSERT_NO_FATAL_FAILURE(GetTestSSLStatus(process()->GetID(), &ssl_status));
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16 ciphersuite = 0xc0a0;
  ssl_status.connection_status =
      (net::SSL_CONNECTION_VERSION_TLS1_2 << net::SSL_CONNECTION_VERSION_SHIFT);
  net::SSLConnectionStatusSetCipherSuite(ciphersuite,
                                         &ssl_status.connection_status);
  SecurityStateModel::SecurityInfoForRequest(url, ssl_status, test_profile,
                                             &security_info);
  EXPECT_FALSE(security_info.is_secure_protocol_and_ciphersuite);
}

}  // namespace
