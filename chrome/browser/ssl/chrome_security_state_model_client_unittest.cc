// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_client.h"

#include "base/command_line.h"
#include "base/test/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/security_state/security_state_model.h"
#include "components/security_state/switches.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kHTTPBadHistogram[] =
    "Security.HTTPBad.UserWarnedAboutSensitiveInput";

// Tests that SecurityInfo flags for subresources with certificate
// errors are reflected in the SecurityStyleExplanations produced by
// ChromeSecurityStateModelClient.
TEST(ChromeSecurityStateModelClientTest,
     GetSecurityStyleForContentWithCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_NONE;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are *not* set when the main resource has major certificate
// errors. If the main resource has certificate errors, it would be
// duplicative/confusing to also report subresources with cert errors.
TEST(ChromeSecurityStateModelClientTest,
     SubresourcesAndMainResourceWithMajorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_NONE;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are set when the main resource has only minor certificate
// errors. Minor errors on the main resource should not hide major
// errors on subresources.
TEST(ChromeSecurityStateModelClientTest,
     SubresourcesAndMainResourceWithMinorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_RAN;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::SecurityStateModel::CONTENT_STATUS_NONE;
  ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                   &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

bool FindSecurityStyleExplanation(
    const std::vector<content::SecurityStyleExplanation>& explanations,
    const char* summary,
    content::SecurityStyleExplanation* explanation) {
  for (const auto& entry : explanations) {
    if (entry.summary == summary) {
      *explanation = entry;
      return true;
    }
  }

  return false;
}

// Test that connection explanations are formated as expected. Note the strings
// are not translated and so will be the same in any locale.
TEST(ChromeSecurityStateModelClientTest, ConnectionExplanation) {
  // Test a modern configuration with a key exchange group.
  security_state::SecurityStateModel::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  {
    content::SecurityStyleExplanations explanations;
    ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, "Secure Connection", &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using a "
        "strong protocol (TLS 1.2), a strong key exchange (ECDHE_RSA with "
        "X25519), and a strong cipher (CHACHA20_POLY1305).",
        explanation.description);
  }

  // Some older cache entries may be missing the key exchange group, despite
  // having a cipher which should supply one.
  security_info.key_exchange_group = 0;
  {
    content::SecurityStyleExplanations explanations;
    ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, "Secure Connection", &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using a "
        "strong protocol (TLS 1.2), a strong key exchange (ECDHE_RSA), and a "
        "strong cipher (CHACHA20_POLY1305).",
        explanation.description);
  }

  // TLS 1.3 ciphers use the key exchange group exclusively.
  net::SSLConnectionStatusSetCipherSuite(0x1301 /* TLS_AES_128_GCM_SHA256 */,
                                         &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_3,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519
  {
    content::SecurityStyleExplanations explanations;
    ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, "Secure Connection", &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using a "
        "strong protocol (TLS 1.3), a strong key exchange (X25519), and a "
        "strong cipher (AES_128_GCM).",
        explanation.description);
  }
}

// Tests that a security level of HTTP_SHOW_WARNING produces a
// content::SecurityStyle of UNAUTHENTICATED, with an explanation.
TEST(ChromeSecurityStateModelClientTest, HTTPWarning) {
  security_state::SecurityStateModel::SecurityInfo security_info;
  content::SecurityStyleExplanations explanations;
  security_info.security_level =
      security_state::SecurityStateModel::HTTP_SHOW_WARNING;
  blink::WebSecurityStyle security_style =
      ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                       &explanations);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated, security_style);
  EXPECT_EQ(1u, explanations.unauthenticated_explanations.size());
}

// Tests that a security level of NONE when there is a password or
// credit card field on HTTP produces a content::SecurityStyle of
// UNAUTHENTICATED, with an info explanation.
TEST(ChromeSecurityStateModelClientTest, HTTPWarningInFuture) {
  security_state::SecurityStateModel::SecurityInfo security_info;
  content::SecurityStyleExplanations explanations;
  security_info.security_level = security_state::SecurityStateModel::NONE;
  security_info.displayed_private_user_data_input_on_http = true;
  blink::WebSecurityStyle security_style =
      ChromeSecurityStateModelClient::GetSecurityStyle(security_info,
                                                       &explanations);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated, security_style);
  EXPECT_EQ(1u, explanations.info_explanations.size());
}

class ChromeSecurityStateModelClientHistogramTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSecurityStateModelClientHistogramTest() {}
  ~ChromeSecurityStateModelClientHistogramTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ChromeSecurityStateModelClient::CreateForWebContents(web_contents());
    client_ = ChromeSecurityStateModelClient::FromWebContents(web_contents());
    navigate_to_http();
  }

 protected:
  ChromeSecurityStateModelClient* client() { return client_; }

  void signal_password() {
    web_contents()->OnPasswordInputShownOnHttp();
    client_->VisibleSecurityStateChanged();
  }

  void navigate_to_http() { NavigateAndCommit(GURL("http://example.test")); }

 private:
  ChromeSecurityStateModelClient* client_;
  DISALLOW_COPY_AND_ASSIGN(ChromeSecurityStateModelClientHistogramTest);
};

// Tests that UMA logs the omnibox warning when security level is
// HTTP_SHOW_WARNING.
TEST_F(ChromeSecurityStateModelClientHistogramTest,
       HTTPOmniboxWarningHistogram) {
  // Show Warning Chip.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpWithPasswordsOrCcWithChip);

  base::HistogramTester histograms;
  signal_password();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, true, 1);

  // Fire again and ensure no sample is recorded.
  signal_password();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, true, 1);

  // Navigate to a new page and ensure a sample is recorded.
  navigate_to_http();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, true, 1);
  signal_password();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, true, 2);
}

// Tests that UMA logs the console warning when security level is NONE.
TEST_F(ChromeSecurityStateModelClientHistogramTest,
       HTTPConsoleWarningHistogram) {
  // Show Neutral for HTTP
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsNeutral);

  base::HistogramTester histograms;
  signal_password();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, false, 1);

  // Fire again and ensure no sample is recorded.
  signal_password();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, false, 1);

  // Navigate to a new page and ensure a sample is recorded.
  navigate_to_http();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, false, 1);
  signal_password();
  histograms.ExpectUniqueSample(kHTTPBadHistogram, false, 2);
}

}  // namespace
