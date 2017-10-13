// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "components/security_state/core/insecure_input_event_data.h"
#include "components/security_state/core/switches.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_state {

namespace {

const char kHttpsUrl[] = "https://foo.test/";
const char kHttpUrl[] = "http://foo.test/";
const char kHttpsSoUrl[] = "https-so://foo.test/";
const char kLocalhostUrl[] = "http://localhost";
const char kFileOrigin[] = "file://example_file";
const char kWssUrl[] = "wss://foo.test/";

// This list doesn't include data: URL, as data: URLs will be explicitly marked
// as not secure.
const char* const kPseudoUrls[] = {
    "blob:http://test/some-guid", "filesystem:http://test/some-guid",
};

bool IsOriginSecure(const GURL& url) {
  return url == kHttpsUrl;
}

class TestSecurityStateHelper {
 public:
  TestSecurityStateHelper()
      : url_(kHttpsUrl),
        cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "sha1_2016.pem")),
        connection_status_(net::SSL_CONNECTION_VERSION_TLS1_2
                           << net::SSL_CONNECTION_VERSION_SHIFT),
        cert_status_(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT),
        displayed_mixed_content_(false),
        contained_mixed_form_(false),
        ran_mixed_content_(false),
        malicious_content_status_(MALICIOUS_CONTENT_STATUS_NONE),
        is_incognito_(false),
        is_error_page_(false) {}
  virtual ~TestSecurityStateHelper() {}

  void SetCertificate(scoped_refptr<net::X509Certificate> cert) {
    cert_ = std::move(cert);
  }
  void set_connection_status(int connection_status) {
    connection_status_ = connection_status;
  }
  void SetCipherSuite(uint16_t ciphersuite) {
    net::SSLConnectionStatusSetCipherSuite(ciphersuite, &connection_status_);
  }
  void AddCertStatus(net::CertStatus cert_status) {
    cert_status_ |= cert_status;
  }
  void set_displayed_mixed_content(bool displayed_mixed_content) {
    displayed_mixed_content_ = displayed_mixed_content;
  }
  void set_contained_mixed_form(bool contained_mixed_form) {
    contained_mixed_form_ = contained_mixed_form;
  }
  void set_ran_mixed_content(bool ran_mixed_content) {
    ran_mixed_content_ = ran_mixed_content;
  }
  void set_malicious_content_status(
      MaliciousContentStatus malicious_content_status) {
    malicious_content_status_ = malicious_content_status;
  }
  void set_password_field_shown(bool password_field_shown) {
    insecure_input_events_.password_field_shown = password_field_shown;
  }
  void set_credit_card_field_edited(bool credit_card_field_edited) {
    insecure_input_events_.credit_card_field_edited = credit_card_field_edited;
  }
  void set_is_incognito(bool is_incognito) { is_incognito_ = is_incognito; }

  void set_is_error_page(bool is_error_page) { is_error_page_ = is_error_page; }

  void set_insecure_field_edit(bool insecure_field_edit) {
    insecure_input_events_.insecure_field_edited = insecure_field_edit;
  }

  void SetUrl(const GURL& url) { url_ = url; }

  std::unique_ptr<VisibleSecurityState> GetVisibleSecurityState() const {
    auto state = base::MakeUnique<VisibleSecurityState>();
    state->connection_info_initialized = true;
    state->url = url_;
    state->certificate = cert_;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->security_bits = 256;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->contained_mixed_form = contained_mixed_form_;
    state->ran_mixed_content = ran_mixed_content_;
    state->malicious_content_status = malicious_content_status_;
    state->is_incognito = is_incognito_;
    state->is_error_page = is_error_page_;
    state->insecure_input_events = insecure_input_events_;
    return state;
  }

  void GetSecurityInfo(SecurityInfo* security_info) const {
    security_state::GetSecurityInfo(
        GetVisibleSecurityState(),
        false /* used policy installed certificate */,
        base::Bind(&IsOriginSecure), security_info);
  }

 private:
  GURL url_;
  scoped_refptr<net::X509Certificate> cert_;
  int connection_status_;
  net::CertStatus cert_status_;
  bool displayed_mixed_content_;
  bool contained_mixed_form_;
  bool ran_mixed_content_;
  MaliciousContentStatus malicious_content_status_;
  bool is_incognito_;
  bool is_error_page_;
  InsecureInputEventData insecure_input_events_;
};

}  // namespace

// Tests that SHA1-signed certificates, when not allowed by policy, downgrade
// the security state of the page to DANGEROUS.
TEST(SecurityStateTest, SHA1Blocked) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  helper.AddCertStatus(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.sha1_in_chain);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that SHA1-signed certificates, when allowed by policy, downgrade the
// security state of the page to NONE.
TEST(SecurityStateTest, SHA1Warning) {
  TestSecurityStateHelper helper;
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.sha1_in_chain);
  EXPECT_EQ(NONE, security_info.security_level);
}

// Tests that SHA1-signed certificates, when allowed by policy, don't interfere
// with the handling of mixed content.
TEST(SecurityStateTest, SHA1WarningMixedContent) {
  TestSecurityStateHelper helper;
  helper.set_displayed_mixed_content(true);
  SecurityInfo security_info1;
  helper.GetSecurityInfo(&security_info1);
  EXPECT_TRUE(security_info1.sha1_in_chain);
  EXPECT_EQ(CONTENT_STATUS_DISPLAYED, security_info1.mixed_content_status);
  EXPECT_EQ(NONE, security_info1.security_level);

  helper.set_displayed_mixed_content(false);
  helper.set_ran_mixed_content(true);
  SecurityInfo security_info2;
  helper.GetSecurityInfo(&security_info2);
  EXPECT_TRUE(security_info2.sha1_in_chain);
  EXPECT_EQ(CONTENT_STATUS_RAN, security_info2.mixed_content_status);
  EXPECT_EQ(DANGEROUS, security_info2.security_level);
}

// Tests that SHA1-signed certificates, when allowed by policy,
// don't interfere with the handling of major cert errors.
TEST(SecurityStateTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.sha1_in_chain);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST(SecurityStateTest, SecureProtocolAndCiphersuite) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_NONE, security_info.obsolete_ssl_status);
}

TEST(SecurityStateTest, NonsecureProtocol) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_1
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_PROTOCOL, security_info.obsolete_ssl_status);
}

TEST(SecurityStateTest, NonsecureCiphersuite) {
  TestSecurityStateHelper helper;
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc0a0;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_KEY_EXCHANGE | net::OBSOLETE_SSL_MASK_CIPHER,
            security_info.obsolete_ssl_status);
}

// Tests that the malware/phishing status is set, and it overrides valid HTTPS.
TEST(SecurityStateTest, MalwareOverride) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);

  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_MALWARE);
  helper.GetSecurityInfo(&security_info);

  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_MALWARE,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that the malware/phishing status is set, even if other connection info
// is not available.
TEST(SecurityStateTest, MalwareWithoutConnectionState) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that pseudo URLs always cause an HTTP_SHOW_WARNING to be shown,
// regardless of whether a password or credit card field was displayed.
TEST(SecurityStateTest, AlwaysWarnOnDataUrls) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("data:text/html,<html>test</html>"));
  helper.set_password_field_shown(false);
  helper.set_credit_card_field_edited(false);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
  EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that FTP URLs always cause an HTTP_SHOW_WARNING to be shown,
// regardless of whether a password or credit card field was displayed.
TEST(SecurityStateTest, AlwaysWarnOnFtpUrls) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("ftp://example.test/"));
  helper.set_password_field_shown(false);
  helper.set_credit_card_field_edited(false);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
  EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that password fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING.
TEST(SecurityStateTest, PasswordFieldWarning) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_password_field_shown(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.insecure_input_events.password_field_shown);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that password fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING on pseudo URLs.
TEST(SecurityStateTest, PasswordFieldWarningOnPseudoUrls) {
  for (const char* const url : kPseudoUrls) {
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(url));
    helper.set_password_field_shown(true);
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.insecure_input_events.password_field_shown);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that credit card fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING.
TEST(SecurityStateTest, CreditCardFieldWarning) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_credit_card_field_edited(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that credit card fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING on pseudo URLs.
TEST(SecurityStateTest, CreditCardFieldWarningOnPseudoUrls) {
  for (const char* const url : kPseudoUrls) {
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(url));
    helper.set_credit_card_field_edited(true);
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.insecure_input_events.credit_card_field_edited);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that neither |password_field_shown| nor
// |credit_card_field_edited| is set when the corresponding
// VisibleSecurityState flags are not set.
TEST(SecurityStateTest, PrivateUserDataNotSet) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
  EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(NONE, security_info.security_level);
}

// Tests that neither |password_field_shown| nor
// |credit_card_field_edited| is set on pseudo URLs when the
// corresponding VisibleSecurityState flags are not set.
TEST(SecurityStateTest, PrivateUserDataNotSetOnPseudoUrls) {
  for (const char* const url : kPseudoUrls) {
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(url));
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
    EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
    EXPECT_EQ(NONE, security_info.security_level);
  }
}

// Tests that |incognito_downgraded_security_level| is set only when the
// corresponding VisibleSecurityState flag is set and the HTTPBad Phase 2
// experiment is enabled.
TEST(SecurityStateTest, IncognitoFlagPropagates) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);

  helper.set_is_incognito(true);
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);
  {
    // Enable the "non-secure-while-incognito" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureWhileIncognito);
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  }
}

// Tests that SSL.MarkHttpAsStatus histogram is updated when security state is
// computed for a page.
TEST(SecurityStateTest, MarkHttpAsStatusHistogram) {
  const char* kHistogramName = "SSL.MarkHttpAsStatus";
  base::HistogramTester histograms;
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  // Ensure histogram recorded correctly when a non-secure password input is
  // found on the page.
  helper.set_password_field_shown(true);
  SecurityInfo security_info;
  histograms.ExpectTotalCount(kHistogramName, 0);
  helper.GetSecurityInfo(&security_info);
  histograms.ExpectUniqueSample(
      kHistogramName, 2 /* HTTP_SHOW_WARNING_ON_SENSITIVE_FIELDS */, 1);

  // Ensure histogram recorded correctly even without a password input.
  helper.set_password_field_shown(false);
  helper.GetSecurityInfo(&security_info);
  histograms.ExpectUniqueSample(
      kHistogramName, 2 /* HTTP_SHOW_WARNING_ON_SENSITIVE_FIELDS */, 2);

  {
    // Test the "non-secure-while-incognito" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureWhileIncognito);

    base::HistogramTester histograms;
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(kHttpUrl));

    // Ensure histogram recorded correctly when the Incognito flag is present.
    helper.set_is_incognito(true);
    SecurityInfo security_info;
    histograms.ExpectTotalCount(kHistogramName, 0);
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.incognito_downgraded_security_level);
    histograms.ExpectUniqueSample(kHistogramName,
                                  4 /* NON_SECURE_WHILE_INCOGNITO */, 1);

    // Ensure histogram recorded correctly even without the Incognito flag.
    helper.set_is_incognito(false);
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.incognito_downgraded_security_level);
    histograms.ExpectUniqueSample(kHistogramName,
                                  4 /* NON_SECURE_WHILE_INCOGNITO */, 2);
  }

  {
    // Test the "non-secure-while-incognito-or-editing" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureWhileIncognitoOrEditing);

    base::HistogramTester histograms;
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(kHttpUrl));

    // Ensure histogram recorded correctly when the Incognito flag is present.
    helper.set_is_incognito(true);
    SecurityInfo security_info;
    histograms.ExpectTotalCount(kHistogramName, 0);
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.incognito_downgraded_security_level);
    histograms.ExpectUniqueSample(
        kHistogramName, 5 /* NON_SECURE_WHILE_INCOGNITO_OR_EDITING */, 1);

    // Ensure histogram recorded correctly when the Insecure Field Edit flag
    // is set.
    helper.set_is_incognito(false);
    helper.set_insecure_field_edit(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.incognito_downgraded_security_level);
    histograms.ExpectUniqueSample(
        kHistogramName, 5 /* NON_SECURE_WHILE_INCOGNITO_OR_EDITING */, 2);

    // Ensure histogram recorded correctly even when neither flag is set.
    helper.set_is_incognito(false);
    helper.set_insecure_field_edit(false);
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.incognito_downgraded_security_level);
    histograms.ExpectUniqueSample(
        kHistogramName, 5 /* NON_SECURE_WHILE_INCOGNITO_OR_EDITING */, 3);
  }

  {
    // Test the "non-secure-after-editing" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureAfterEditing);

    base::HistogramTester histograms;
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(kHttpUrl));

    // Ensure histogram recorded correctly when the editing flag is present.
    helper.set_insecure_field_edit(true);
    SecurityInfo security_info;
    histograms.ExpectTotalCount(kHistogramName, 0);
    helper.GetSecurityInfo(&security_info);
    histograms.ExpectUniqueSample(kHistogramName,
                                  3 /*  NON_SECURE_AFTER_EDITING */, 1);

    // Ensure histogram recorded correctly even without the editing flag.
    helper.set_insecure_field_edit(false);
    helper.GetSecurityInfo(&security_info);
    histograms.ExpectUniqueSample(kHistogramName,
                                  3 /*  NON_SECURE_AFTER_EDITING */, 2);
  }

  {
    // Test the "dangerous" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsDangerous);

    base::HistogramTester histograms;
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(kHttpUrl));

    // Ensure histogram recorded correctly.
    SecurityInfo security_info;
    histograms.ExpectTotalCount(kHistogramName, 0);
    helper.GetSecurityInfo(&security_info);
    histograms.ExpectUniqueSample(kHistogramName, 1 /* NON_SECURE */, 1);
  }
}

TEST(SecurityStateTest, DetectSubjectAltName) {
  TestSecurityStateHelper helper;

  // Ensure subjectAltName is detected as present when the cert includes it.
  SecurityInfo san_security_info;
  helper.GetSecurityInfo(&san_security_info);
  EXPECT_FALSE(san_security_info.cert_missing_subject_alt_name);

  // Ensure subjectAltName is detected as missing when the cert doesn't
  // include it.
  scoped_refptr<net::X509Certificate> cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "salesforce_com_test.pem");
  ASSERT_TRUE(cert);
  helper.SetCertificate(std::move(cert));

  SecurityInfo no_san_security_info;
  helper.GetSecurityInfo(&no_san_security_info);
  EXPECT_TRUE(no_san_security_info.cert_missing_subject_alt_name);
}

// Tests that a mixed form is reflected in the SecurityInfo.
TEST(SecurityStateTest, MixedForm) {
  TestSecurityStateHelper helper;

  SecurityInfo no_mixed_form_security_info;
  helper.GetSecurityInfo(&no_mixed_form_security_info);
  EXPECT_FALSE(no_mixed_form_security_info.contained_mixed_form);

  helper.set_contained_mixed_form(true);

  SecurityInfo mixed_form_security_info;
  helper.GetSecurityInfo(&mixed_form_security_info);
  EXPECT_TRUE(mixed_form_security_info.contained_mixed_form);
  EXPECT_EQ(CONTENT_STATUS_NONE, mixed_form_security_info.mixed_content_status);
  EXPECT_EQ(NONE, mixed_form_security_info.security_level);

  helper.set_ran_mixed_content(true);
  SecurityInfo mixed_form_and_active_security_info;
  helper.GetSecurityInfo(&mixed_form_and_active_security_info);
  EXPECT_TRUE(mixed_form_and_active_security_info.contained_mixed_form);
  EXPECT_EQ(CONTENT_STATUS_RAN,
            mixed_form_and_active_security_info.mixed_content_status);
  EXPECT_EQ(DANGEROUS, mixed_form_and_active_security_info.security_level);
}

// Tests that a field edit is reflected in the SecurityInfo.
TEST(SecurityStateTest, FieldEdit) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  SecurityInfo no_field_edit_security_info;
  helper.GetSecurityInfo(&no_field_edit_security_info);
  EXPECT_FALSE(
      no_field_edit_security_info.insecure_input_events.insecure_field_edited);

  helper.set_insecure_field_edit(true);

  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.insecure_input_events.insecure_field_edited);

  {
    // Test that no warning is raised in the "non-secure-while-incognito"
    // configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureWhileIncognito);

    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(NONE, security_info.security_level);
  }

  {
    // Test the "non-secure-after-editing" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureAfterEditing);

    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }

  {
    // Test the "non-secure-while-incognito-or-editing" configuration.
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        security_state::switches::kMarkHttpAs,
        security_state::switches::kMarkHttpAsNonSecureWhileIncognitoOrEditing);

    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests IsSchemeCryptographic function.
TEST(SecurityStateTest, CryptographicSchemeUrl) {
  // HTTPS is a cryptographic scheme.
  EXPECT_TRUE(IsSchemeCryptographic(GURL(kHttpsUrl)));
  // HTTPS-SO is a cryptographic scheme.
  EXPECT_TRUE(IsSchemeCryptographic(GURL(kHttpsSoUrl)));
  // WSS is a cryptographic scheme.
  EXPECT_TRUE(IsSchemeCryptographic(GURL(kWssUrl)));
  // HTTP is not a cryptographic scheme.
  EXPECT_FALSE(IsSchemeCryptographic(GURL(kHttpUrl)));
  // Return true only for valid |url|
  EXPECT_FALSE(IsSchemeCryptographic(GURL("https://")));
}

// Tests IsOriginLocalhostOrFile function.
TEST(SecurityStateTest, LocalhostOrFileUrl) {
  EXPECT_TRUE(IsOriginLocalhostOrFile(GURL(kLocalhostUrl)));
  EXPECT_TRUE(IsOriginLocalhostOrFile(GURL(kFileOrigin)));
  EXPECT_FALSE(IsOriginLocalhostOrFile(GURL(kHttpsUrl)));
}

// Tests IsSslCertificateValid function.
TEST(SecurityStateTest, SslCertificateValid) {
  EXPECT_TRUE(IsSslCertificateValid(SecurityLevel::SECURE));
  EXPECT_TRUE(IsSslCertificateValid(SecurityLevel::EV_SECURE));
  EXPECT_TRUE(
      IsSslCertificateValid(SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT));

  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::NONE));
  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::DANGEROUS));
  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::HTTP_SHOW_WARNING));
}

// Tests that HTTP_SHOW_WARNING is not set in incognito for error pages.
TEST(SecurityStateTest, IncognitoErrorPage) {
  // Enable the "non-secure-while-incognito" configuration.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsNonSecureWhileIncognito);
  TestSecurityStateHelper helper;
  SecurityInfo security_info;
  helper.SetUrl(GURL("http://nonexistent.test"));
  helper.set_is_incognito(true);
  helper.set_is_error_page(true);
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(SecurityLevel::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);

  // Sanity-check that if it's not an error page, the security level is
  // downgraded.
  helper.set_is_error_page(false);
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(SecurityLevel::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
}

}  // namespace security_state
