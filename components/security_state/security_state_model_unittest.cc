// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/security_state_model.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "components/security_state/security_state_model_client.h"
#include "components/security_state/switches.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_state {

namespace {

const char kHttpsUrl[] = "https://foo.test";
const char kHttpUrl[] = "http://foo.test";

class TestSecurityStateModelClient : public SecurityStateModelClient {
 public:
  TestSecurityStateModelClient()
      : url_(kHttpsUrl),
        connection_status_(net::SSL_CONNECTION_VERSION_TLS1_2
                           << net::SSL_CONNECTION_VERSION_SHIFT),
        cert_status_(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT),
        displayed_mixed_content_(false),
        ran_mixed_content_(false),
        fails_malware_check_(false),
        displayed_password_field_on_http_(false),
        displayed_credit_card_field_on_http_(false) {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "sha1_2016.pem");
  }
  ~TestSecurityStateModelClient() override {}

  void set_connection_status(int connection_status) {
    connection_status_ = connection_status;
  }
  void SetCipherSuite(uint16_t ciphersuite) {
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
  void set_fails_malware_check(bool fails_malware_check) {
    fails_malware_check_ = fails_malware_check;
  }
  void set_displayed_password_field_on_http(
      bool displayed_password_field_on_http) {
    displayed_password_field_on_http_ = displayed_password_field_on_http;
  }
  void set_displayed_credit_card_field_on_http(
      bool displayed_credit_card_field_on_http) {
    displayed_credit_card_field_on_http_ = displayed_credit_card_field_on_http;
  }

  void UseHttpUrl() { url_ = GURL(kHttpUrl); }

  // SecurityStateModelClient:
  void GetVisibleSecurityState(
      SecurityStateModel::VisibleSecurityState* state) override {
    state->connection_info_initialized = true;
    state->url = url_;
    state->certificate = cert_;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->security_bits = 256;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->ran_mixed_content = ran_mixed_content_;
    state->fails_malware_check = fails_malware_check_;
    state->displayed_password_field_on_http = displayed_password_field_on_http_;
    state->displayed_credit_card_field_on_http =
        displayed_credit_card_field_on_http_;
  }

  bool UsedPolicyInstalledCertificate() override { return false; }

  bool IsOriginSecure(const GURL& url) override {
    return url_ == GURL(kHttpsUrl);
  }

 private:
  GURL url_;
  scoped_refptr<net::X509Certificate> cert_;
  int connection_status_;
  net::CertStatus cert_status_;
  bool displayed_mixed_content_;
  bool ran_mixed_content_;
  bool fails_malware_check_;
  bool displayed_password_field_on_http_;
  bool displayed_credit_card_field_on_http_;
};

// Tests that SHA1-signed certificates expiring in 2016 downgrade the
// security state of the page.
TEST(SecurityStateModelTest, SHA1Warning) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of mixed
// content.
TEST(SecurityStateModelTest, SHA1WarningMixedContent) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  client.SetDisplayedMixedContent(true);
  SecurityStateModel::SecurityInfo security_info1;
  model.GetSecurityInfo(&security_info1);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info1.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::CONTENT_STATUS_DISPLAYED,
            security_info1.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::NONE, security_info1.security_level);

  client.SetDisplayedMixedContent(false);
  client.SetRanMixedContent(true);
  SecurityStateModel::SecurityInfo security_info2;
  model.GetSecurityInfo(&security_info2);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info2.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::CONTENT_STATUS_RAN,
            security_info2.mixed_content_status);
  EXPECT_EQ(SecurityStateModel::DANGEROUS, security_info2.security_level);
}

// Tests that SHA1 warnings don't interfere with the handling of major
// cert errors.
TEST(SecurityStateModelTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  client.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_EQ(SecurityStateModel::DEPRECATED_SHA1_MINOR,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::DANGEROUS, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST(SecurityStateModelTest, SecureProtocolAndCiphersuite) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_NONE, security_info.obsolete_ssl_status);
}

TEST(SecurityStateModelTest, NonsecureProtocol) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_1
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_PROTOCOL, security_info.obsolete_ssl_status);
}

TEST(SecurityStateModelTest, NonsecureCiphersuite) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc0a0;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_KEY_EXCHANGE | net::OBSOLETE_SSL_MASK_CIPHER,
            security_info.obsolete_ssl_status);
}

// Tests that the malware/phishing status is set, and it overrides valid HTTPS.
TEST(SecurityStateModelTest, MalwareOverride) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  client.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  client.SetCipherSuite(ciphersuite);
  client.set_fails_malware_check(true);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.fails_malware_check);
  EXPECT_EQ(SecurityStateModel::DANGEROUS, security_info.security_level);
}

// Tests that the malware/phishing status is set, even if other connection info
// is not available.
TEST(SecurityStateModelTest, MalwareWithoutCOnnectionState) {
  TestSecurityStateModelClient client;
  SecurityStateModel model;
  model.SetClient(&client);
  client.set_fails_malware_check(true);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.fails_malware_check);
  EXPECT_EQ(SecurityStateModel::DANGEROUS, security_info.security_level);
}

// Tests that password fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING when the command-line switch is set.
TEST(SecurityStateModelTest, PasswordFieldWarning) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMarkHttpAs,
      switches::kMarkHttpWithPasswordsOrCcWithChip);
  TestSecurityStateModelClient client;
  client.UseHttpUrl();
  SecurityStateModel model;
  model.SetClient(&client);
  client.set_displayed_password_field_on_http(true);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_private_user_data_input_on_http);
  EXPECT_EQ(SecurityStateModel::HTTP_SHOW_WARNING,
            security_info.security_level);
}

// Tests that credit card fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING when the command-line switch is set.
TEST(SecurityStateModelTest, CreditCardFieldWarning) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMarkHttpAs,
      switches::kMarkHttpWithPasswordsOrCcWithChip);
  TestSecurityStateModelClient client;
  client.UseHttpUrl();
  SecurityStateModel model;
  model.SetClient(&client);
  client.set_displayed_credit_card_field_on_http(true);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_private_user_data_input_on_http);
  EXPECT_EQ(SecurityStateModel::HTTP_SHOW_WARNING,
            security_info.security_level);
}

// Tests that neither password nor credit fields cause the security
// level to be downgraded to HTTP_SHOW_WARNING when the command-line switch
// is NOT set.
TEST(SecurityStateModelTest, HttpWarningNotSetWithoutSwitch) {
  TestSecurityStateModelClient client;
  client.UseHttpUrl();
  SecurityStateModel model;
  model.SetClient(&client);
  client.set_displayed_password_field_on_http(true);
  client.set_displayed_credit_card_field_on_http(true);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.displayed_private_user_data_input_on_http);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
}

// Tests that |displayed_private_user_data_input_on_http| is not set
// when the corresponding VisibleSecurityState flags are not set.
TEST(SecurityStateModelTest, PrivateUserDataNotSet) {
  TestSecurityStateModelClient client;
  client.UseHttpUrl();
  SecurityStateModel model;
  model.SetClient(&client);
  SecurityStateModel::SecurityInfo security_info;
  model.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.displayed_private_user_data_input_on_http);
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
}

// Tests that SSL.MarkHttpAsStatus histogram is updated when security state is
// computed for a page containing a password field on HTTP.
TEST(SecurityStateModelTest, MarkHttpAsStatusHistogram) {
  const char* kHistogramName = "SSL.MarkHttpAsStatus";
  base::HistogramTester histograms;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kMarkHttpAs, switches::kMarkHttpWithPasswordsOrCcWithChip);
  TestSecurityStateModelClient client;
  client.UseHttpUrl();
  SecurityStateModel model;
  model.SetClient(&client);
  client.set_displayed_password_field_on_http(true);
  SecurityStateModel::SecurityInfo security_info;
  histograms.ExpectTotalCount(kHistogramName, 0);
  model.GetSecurityInfo(&security_info);
  histograms.ExpectUniqueSample(kHistogramName, 2 /* HTTP_SHOW_WARNING */, 1);
}

}  // namespace

}  // namespace security_state
