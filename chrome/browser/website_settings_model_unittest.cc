// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/website_settings_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/cert_store.h"
#include "content/public/common/ssl_status.h"
#include "net/base/cert_status_flags.h"
#include "net/base/ssl_connection_status_flags.h"
#include "net/base/test_certificate_data.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SSLStatus;

namespace {

// SSL cipher suite like specified in RFC5246 Appendix A.5. "The Cipher Suite".
static int TLS_RSA_WITH_AES_256_CBC_SHA256 = 0x3D;

int SetSSLVersion(int connection_status, int version) {
  // Clear SSL version bits (Bits 20, 21 and 22).
  connection_status &=
      ~(net::SSL_CONNECTION_VERSION_MASK << net::SSL_CONNECTION_VERSION_SHIFT);
  int bitmask = version << net::SSL_CONNECTION_VERSION_SHIFT;
  return bitmask | connection_status;
}

int SetSSLCipherSuite(int connection_status, int cipher_suite) {
  // Clear cipher suite bits (the 16 lowest bits).
  connection_status &= ~net::SSL_CONNECTION_CIPHERSUITE_MASK;
  return cipher_suite | connection_status;
}

}  // namespace

class WebsiteSettingsModelTest : public testing::Test {
 public:
  WebsiteSettingsModelTest() : profile_(new TestingProfile()),
                               cert_id_(0),
                               ev_cert_id_(0),
                               bad_cert_id_(0) {
    InitCertStore();
  }

  void InitCertStore() {
    // TODO(markusheintz): I wish there was an easy way to create a mock
    // CertStore.
    int render_process_host_id = 1;
    base::Time start_date = base::Time::Now();
    base::Time expiration_date = base::Time::FromInternalValue(
        start_date.ToInternalValue() + base::Time::kMicrosecondsPerWeek);

    net::X509Certificate* cert =
        new net::X509Certificate("subject",
                                 "issuer",
                                 start_date,
                                 expiration_date);
    cert_id_ = CertStore::GetInstance()->StoreCert(cert,
                                                   render_process_host_id);
    cert = net::X509Certificate::CreateFromBytes(
        reinterpret_cast<const char*>(google_der), sizeof(google_der));
    ev_cert_id_ = CertStore::GetInstance()->StoreCert(cert,
                                                      render_process_host_id);
    cert = new net::X509Certificate("subject",
                                    "issuer",
                                    base::Time(),
                                    base::Time());
    bad_cert_id_ = CertStore::GetInstance()->StoreCert(cert,
                                                       render_process_host_id);
  }

  int cert_id() const { return cert_id_; }

  int ev_cert_id() const { return ev_cert_id_; }

  int bad_cert_id() const { return bad_cert_id_; }

  Profile* profile() const { return profile_.get(); }

 private:
  scoped_ptr<Profile> profile_;
  int cert_id_;
  int ev_cert_id_;
  int bad_cert_id_;
};

TEST_F(WebsiteSettingsModelTest, HTTPConnection) {
  GURL url = GURL("http://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_UNAUTHENTICATED;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_UNENCRYPTED,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_NO_CERT,
            model->site_identity_status());
  EXPECT_EQ(string16(), model->organization_name());
}

TEST_F(WebsiteSettingsModelTest, HTTPSConnection) {
  GURL url = GURL("https://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl.cert_id = cert_id();
  ssl.cert_status = 0;
  ssl.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl.connection_status = status;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_ENCRYPTED,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_CERT,
            model->site_identity_status());
  EXPECT_EQ(string16(), model->organization_name());
}

TEST_F(WebsiteSettingsModelTest, HTTPSMixedContent) {
  GURL url = GURL("https://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl.cert_id = cert_id();
  ssl.cert_status = 0;
  ssl.security_bits = 81;  // No error if > 80.
  ssl.content_status = SSLStatus::DISPLAYED_INSECURE_CONTENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl.connection_status = status;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_MIXED_CONTENT,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_CERT,
            model->site_identity_status());
  EXPECT_EQ(string16(), model->organization_name());
}

TEST_F(WebsiteSettingsModelTest, HTTPSEVCert) {
  GURL url = GURL("https://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl.cert_id = ev_cert_id();
  ssl.cert_status = net::CERT_STATUS_IS_EV;
  ssl.security_bits = 81;  // No error if > 80.
  ssl.content_status = SSLStatus::DISPLAYED_INSECURE_CONTENT;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl.connection_status = status;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_MIXED_CONTENT,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_EV_CERT,
            model->site_identity_status());
  EXPECT_EQ(UTF8ToUTF16("Google Inc"), model->organization_name());
}

TEST_F(WebsiteSettingsModelTest, HTTPSBadCertificate) {
  GURL url = GURL("https://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl.cert_id = bad_cert_id();
  ssl.cert_status = net::CERT_STATUS_DATE_INVALID;
  ssl.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl.connection_status = status;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_ENCRYPTED,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_ERROR,
            model->site_identity_status());
  EXPECT_EQ(string16(), model->organization_name());
}

TEST_F(WebsiteSettingsModelTest, HTTPSRevocationError) {
  GURL url = GURL("https://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl.cert_id = cert_id();
  ssl.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  ssl.security_bits = 81;  // No error if > 80.
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl.connection_status = status;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_ENCRYPTED,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN,
            model->site_identity_status());
  EXPECT_EQ(string16(), model->organization_name());
}

TEST_F(WebsiteSettingsModelTest, HTTPSConnectionError) {
  GURL url = GURL("https://www.example.com");

  SSLStatus ssl;
  ssl.security_style = content::SECURITY_STYLE_AUTHENTICATED;
  ssl.cert_id = cert_id();
  ssl.cert_status = 0;
  ssl.security_bits = 1;
  int status = 0;
  status = SetSSLVersion(status, net::SSL_CONNECTION_VERSION_TLS1);
  status = SetSSLCipherSuite(status, TLS_RSA_WITH_AES_256_CBC_SHA256);
  ssl.connection_status = status;

  scoped_ptr<WebsiteSettingsModel> model(
      new WebsiteSettingsModel(profile(), url, ssl));
  EXPECT_EQ(WebsiteSettingsModel::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,
            model->site_connection_status());
  EXPECT_EQ(WebsiteSettingsModel::SITE_IDENTITY_STATUS_CERT,
            model->site_identity_status());
  EXPECT_EQ(string16(), model->organization_name());
}
