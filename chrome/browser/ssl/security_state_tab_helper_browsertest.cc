// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum CertificateStatus { VALID_CERTIFICATE, INVALID_CERTIFICATE };

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

const char kTestCertificateIssuerName[] = "Test Root CA";

// Inject a script into every frame in the page. Used by tests that check for
// visible password fields to wait for notifications about these
// fields. Notifications about visible password fields are queued at the end of
// the event loop, so waiting for a dummy script to run ensures that these
// notifications have been sent.
void InjectScript(content::WebContents* contents) {
  // Any frame in the page might have a password field, so inject scripts into
  // all of them to ensure that notifications from all of them have been sent.
  for (auto* frame : contents->GetAllFrames()) {
    bool js_result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        frame, "window.domAutomationController.send(true);", &js_result));
    EXPECT_TRUE(js_result);
  }
}

// Gets the Insecure Input Events from the entry's SSLStatus user data.
security_state::InsecureInputEventData GetInputEvents(
    content::NavigationEntry* entry) {
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          entry->GetSSL().user_data.get());
  if (input_events)
    return *input_events->input_events();

  return security_state::InsecureInputEventData();
}

// Stores the Insecure Input Events to the entry's SSLStatus user data.
void SetInputEvents(content::NavigationEntry* entry,
                    security_state::InsecureInputEventData events) {
  security_state::SSLStatus& ssl = entry->GetSSL();
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (!input_events) {
    ssl.user_data =
        std::make_unique<security_state::SSLStatusInputEventData>(events);
  } else {
    *input_events->input_events() = events;
  }
}

void SimulateCreditCardFieldEdit(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  security_state::InsecureInputEventData input_events = GetInputEvents(entry);
  input_events.credit_card_field_edited = true;
  SetInputEvents(entry, input_events);
  contents->DidChangeVisibleSecurityState();
}

void SimulatePasswordFieldShown(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  security_state::InsecureInputEventData input_events = GetInputEvents(entry);
  input_events.password_field_shown = true;
  SetInputEvents(entry, input_events);
  contents->DidChangeVisibleSecurityState();
}

// A WebContentsObserver useful for testing the DidChangeVisibleSecurityState()
// method: it keeps track of the latest security style and explanation that was
// fired.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        latest_security_style_(blink::kWebSecurityStyleUnknown) {}
  ~SecurityStyleTestObserver() override {}

  void DidChangeVisibleSecurityState() override {
    content::SecurityStyleExplanations explanations;
    latest_security_style_ = web_contents()->GetDelegate()->GetSecurityStyle(
        web_contents(), &explanations);
    latest_explanations_ = explanations;
    run_loop_.Quit();
  }

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

  blink::WebSecurityStyle latest_security_style() const {
    return latest_security_style_;
  }

  const content::SecurityStyleExplanations& latest_explanations() const {
    return latest_explanations_;
  }

  void ClearLatestSecurityStyleAndExplanations() {
    latest_security_style_ = blink::kWebSecurityStyleUnknown;
    latest_explanations_ = content::SecurityStyleExplanations();
  }

 private:
  blink::WebSecurityStyle latest_security_style_;
  content::SecurityStyleExplanations latest_explanations_;
  base::RunLoop run_loop_;
  DISALLOW_COPY_AND_ASSIGN(SecurityStyleTestObserver);
};

// Check that |observer|'s latest event was for an expired certificate
// and that it saw the proper SecurityStyle and explanations.
void CheckBrokenSecurityStyle(const SecurityStyleTestObserver& observer,
                              int error,
                              Browser* browser,
                              net::X509Certificate* expected_cert) {
  EXPECT_EQ(blink::kWebSecurityStyleInsecure, observer.latest_security_style());

  const content::SecurityStyleExplanations& expired_explanation =
      observer.latest_explanations();
  EXPECT_EQ(0u, expired_explanation.neutral_explanations.size());
  ASSERT_EQ(1u, expired_explanation.insecure_explanations.size());
  EXPECT_FALSE(expired_explanation.pkp_bypassed);
  EXPECT_TRUE(expired_explanation.info_explanations.empty());

  // Check that the summary and description are as expected.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
            expired_explanation.insecure_explanations[0].summary);

  base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(error));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
            expired_explanation.insecure_explanations[0].description);

  // Check the associated certificate.
  net::X509Certificate* cert = browser->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetController()
                                   .GetActiveEntry()
                                   ->GetSSL()
                                   .certificate.get();
  EXPECT_TRUE(cert->Equals(expected_cert));
  EXPECT_TRUE(!!expired_explanation.insecure_explanations[0].certificate);
}

// Checks that the given |explanation| contains an appropriate
// explanation if the certificate status is valid.
void CheckSecureCertificateExplanation(
    const content::SecurityStyleExplanation& explanation,
    Browser* browser,
    net::X509Certificate* expected_cert) {
  ASSERT_EQ(kTestCertificateIssuerName,
            expected_cert->issuer().GetDisplayName());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
            explanation.summary);
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION,
                                base::UTF8ToUTF16(kTestCertificateIssuerName)),
      explanation.description);
  net::X509Certificate* cert = browser->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetController()
                                   .GetActiveEntry()
                                   ->GetSSL()
                                   .certificate.get();
  EXPECT_TRUE(cert->Equals(expected_cert));
  EXPECT_TRUE(!!explanation.certificate);
}

// Checks that the given |explanation| contains an appropriate
// explanation that the connection is secure.
void CheckSecureConnectionExplanation(
    const content::SecurityStyleExplanation& explanation,
    Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);

  const char *protocol, *key_exchange, *cipher, *mac;
  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  net::SSLVersionToString(&protocol, ssl_version);
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_STRONG_SSL_SUMMARY,
                                      base::ASCIIToUTF16(protocol)),
            explanation.summary);

  bool is_aead, is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(security_info.connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  EXPECT_TRUE(is_aead);
  EXPECT_EQ(nullptr, mac);  // The default secure cipher does not have a MAC.
  EXPECT_FALSE(is_tls13);   // The default secure cipher is not TLS 1.3.

  base::string16 key_exchange_name = base::ASCIIToUTF16(key_exchange);
  if (security_info.key_exchange_group != 0) {
    key_exchange_name = l10n_util::GetStringFUTF16(
        IDS_SSL_KEY_EXCHANGE_WITH_GROUP, key_exchange_name,
        base::ASCIIToUTF16(
            SSL_get_curve_name(security_info.key_exchange_group)));
  }

  std::vector<base::string16> description_replacements;
  description_replacements.push_back(base::ASCIIToUTF16(protocol));
  description_replacements.push_back(key_exchange_name);
  description_replacements.push_back(base::ASCIIToUTF16(cipher));
  base::string16 secure_description = l10n_util::GetStringFUTF16(
      IDS_STRONG_SSL_DESCRIPTION, description_replacements, nullptr);

  EXPECT_EQ(secure_description, base::ASCIIToUTF16(explanation.description));
}

// Checks that the given |explanation| contains an appropriate
// explanation that the subresources are secure.
void CheckSecureSubresourcesExplanation(
    const content::SecurityStyleExplanation& explanation) {
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_SUMMARY),
            explanation.summary);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_DESCRIPTION),
            explanation.description);
}

void CheckSecurityInfoForSecure(
    content::WebContents* contents,
    security_state::SecurityLevel expect_security_level,
    bool expect_sha1_in_chain,
    security_state::ContentStatus expect_mixed_content_status,
    bool pkp_bypassed,
    bool expect_cert_error) {
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(expect_security_level, security_info.security_level);
  EXPECT_EQ(expect_sha1_in_chain, security_info.sha1_in_chain);
  EXPECT_EQ(expect_mixed_content_status, security_info.mixed_content_status);
  EXPECT_TRUE(security_info.scheme_is_cryptographic);
  EXPECT_EQ(pkp_bypassed, security_info.pkp_bypassed);
  EXPECT_EQ(expect_cert_error,
            net::IsCertStatusError(security_info.cert_status));
  EXPECT_GT(security_info.security_bits, 0);
  EXPECT_TRUE(!!security_info.certificate);
}

void CheckSecurityInfoForNonSecure(content::WebContents* contents) {
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.sha1_in_chain);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_FALSE(security_info.scheme_is_cryptographic);
  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(-1, security_info.security_bits);
  EXPECT_FALSE(!!security_info.certificate);
}

void ProceedThroughInterstitial(content::WebContents* tab) {
  content::InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(&tab->GetController()));
  interstitial_page->Proceed();
  observer.Wait();
}

void GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair,
    std::string* replacement_path) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  net::test_server::GetFilePathWithReplacements(
      original_file_path, replacement_text, replacement_path);
}

GURL GetURLWithNonLocalHostname(net::EmbeddedTestServer* server,
                                const std::string& path) {
  GURL::Replacements replace_host;
  replace_host.SetHostStr("example.test");
  return server->GetURL(path).ReplaceComponents(replace_host);
}

class SecurityStateTabHelperTest : public CertVerifierBrowserTest {
 public:
  SecurityStateTabHelperTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(https_server_.Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    // Allow tests to trigger error pages.
    host_resolver()->AddSimulatedFailure("nonexistent.test");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CertVerifierBrowserTest::SetUpCommandLine(command_line);
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  }

 protected:
  void SetUpMockCertVerifierForHttpsServer(net::CertStatus cert_status,
                                           int net_result) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root = true;
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;

    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  // Navigates to an empty page and runs |javascript| to create a URL with with
  // a scheme of |scheme|. If |expect_warning| is true, expects a password
  // warning.
  void TestPasswordFieldOnBlobOrFilesystemURL(const std::string& scheme,
                                              const std::string& javascript,
                                              bool expect_warning) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(contents);

    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(contents);
    ASSERT_TRUE(helper);

    ui_test_utils::NavigateToURL(
        browser(),
        GetURLWithNonLocalHostname(embedded_test_server(), "/empty.html"));

    // Create a URL and navigate to it.
    std::string blob_or_filesystem_url;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, javascript, &blob_or_filesystem_url));
    EXPECT_TRUE(GURL(blob_or_filesystem_url).SchemeIs(scheme));

    ui_test_utils::NavigateToURL(browser(), GURL(blob_or_filesystem_url));
    InjectScript(contents);
    security_state::SecurityInfo security_info;
    helper->GetSecurityInfo(&security_info);

    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    ASSERT_TRUE(entry);

    security_state::InsecureInputEventData input_events = GetInputEvents(entry);
    if (expect_warning) {
      EXPECT_EQ(security_state::HTTP_SHOW_WARNING,
                security_info.security_level);
      EXPECT_TRUE(input_events.password_field_shown);
    } else {
      EXPECT_EQ(security_state::NONE, security_info.security_level);
      EXPECT_FALSE(input_events.password_field_shown);
    }
  }

  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperTest);
};

// Same as SecurityStateTabHelperTest, but with Incognito enabled.
class SecurityStateTabHelperIncognitoTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateTabHelperIncognitoTest() : SecurityStateTabHelperTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SecurityStateTabHelperTest::SetUpCommandLine(command_line);
    // Test should run Incognito.
    command_line->AppendSwitch(switches::kIncognito);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperIncognitoTest);
};

class DidChangeVisibleSecurityStateTest : public InProcessBrowserTest {
 public:
  DidChangeVisibleSecurityStateTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DidChangeVisibleSecurityStateTest);
};

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, HttpPage) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.sha1_in_chain);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_FALSE(security_info.scheme_is_cryptographic);
  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_FALSE(!!security_info.certificate);
  EXPECT_EQ(-1, security_info.security_bits);
  EXPECT_EQ(0, security_info.connection_status);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, HttpsPage) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
}

// Tests that interstitial.ssl.visited_site_after_warning is being logged to
// correctly.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, UMALogsVisitsAfterWarning) {
  const char kHistogramName[] = "interstitial.ssl.visited_site_after_warning";
  base::HistogramTester histograms;
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  // Histogram shouldn't log before clicking through interstitial.
  histograms.ExpectTotalCount(kHistogramName, 0);
  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Histogram should log after clicking through.
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectBucketCount(kHistogramName, true, 1);
}

// Tests that interstitial.ssl.visited_site_after_warning is not being logged
// to on errors that do not trigger a full site interstitial.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, UMADoesNotLogOnMinorError) {
  const char kHistogramName[] = "interstitial.ssl.visited_site_after_warning";
  base::HistogramTester histograms;
  SetUpMockCertVerifierForHttpsServer(
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION, net::OK);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  histograms.ExpectTotalCount(kHistogramName, 0);
}

// Test security state after clickthrough for a SHA-1 certificate that is
// blocked by default.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SHA1CertificateBlocked) {
  SetUpMockCertVerifierForHttpsServer(
      net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
          net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM,
      net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  const content::SecurityStyleExplanations& interstitial_explanation =
      observer.latest_explanations();
  ASSERT_EQ(2u, interstitial_explanation.insecure_explanations.size());
  ASSERT_EQ(0u, interstitial_explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            interstitial_explanation.insecure_explanations[0].summary);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  const content::SecurityStyleExplanations& page_explanation =
      observer.latest_explanations();
  ASSERT_EQ(2u, page_explanation.insecure_explanations.size());
  ASSERT_EQ(0u, page_explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            page_explanation.insecure_explanations[0].summary);
}

// Test security state for a SHA-1 certificate that is allowed by policy.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, SHA1CertificateWarning) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();

  ASSERT_EQ(0u, explanation.insecure_explanations.size());
  ASSERT_EQ(1u, explanation.neutral_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            explanation.neutral_explanations[0].summary);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, MixedContent) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, false, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement("/ssl/page_runs_insecure_content.html",
                                        replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_RAN,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false,
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content in an iframe.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/title1.html"));
  host_port_pair.set_host("different-host.test");
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe.html", host_port_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_RAN,
      false, false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       ActiveContentWithCertErrors) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page and simulate active content with
  // certificate errors.
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  entry->GetSSL().content_status |=
      content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::CONTENT_STATUS_RAN,
            security_info.content_with_cert_errors_status);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PassiveContentWithCertErrors) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page and simulate passive content with
  // certificate errors.
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  entry->GetSSL().content_status |=
      content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED,
            security_info.content_with_cert_errors_status);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       ActiveAndPassiveContentWithCertErrors) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page and simulate active and passive content
  // with certificate errors.
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  entry->GetSSL().content_status |=
      content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS;
  entry->GetSSL().content_status |=
      content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS;

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
            security_info.content_with_cert_errors_status);
}

// Same as SecurityStateTabHelperTest.ActiveAndPassiveContentWithCertErrors but
// with a SHA1 cert.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, MixedContentWithSHA1Cert) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::NONE, true, security_state::CONTENT_STATUS_DISPLAYED,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement("/ssl/page_runs_insecure_content.html",
                                        replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_RAN,
      false, false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html", replacement_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true,
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN, false,
      false /* expect cert status error */);
}

// Tests that the Content Security Policy block-all-mixed-content
// directive stops mixed content from running.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, MixedContentStrictBlocking) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page that tries to run mixed content in an
  // iframe, with strict mixed content blocking.
  std::string replacement_path;
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/title1.html"));
  host_port_pair.set_host("different-host.test");
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe_with_strict_blocking.html",
      host_port_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, BrokenHTTPS) {
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  // Navigate to a broken HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, false,
      security_state::CONTENT_STATUS_DISPLAYED, false,
      true /* expect cert status error */);
}

// Tests that the security level of data: URLs is always downgraded to
// HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedOnDataUrl) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,<html></html>"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Ensure that WebContentsObservers don't show an incorrect Form Not Secure
  // explanation. Regression test for https://crbug.com/691412.
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::SSLStatus::NORMAL_CONTENT, entry->GetSSL().content_status);
}

// Tests the security level and malicious content status for password reuse
// threat type.
IN_PROC_BROWSER_TEST_F(
    SecurityStateTabHelperTest,
    VerifyPasswordReuseMaliciousContentAndSecurityLevelWhenFeatureEnabled) {
  // Setup https server. This makes sure that the DANGEROUS security level is
  // not caused by any certificate error rather than the password reuse SB
  // threat type.
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      safe_browsing::kGoogleBrandedPhishingWarning);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  // Update security state of the current page to match
  // SB_THREAT_TYPE_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  service->ShowModalWarning(contents, "unused-token");
  observer.WaitForDidChangeVisibleSecurityState();

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates a Gaia password change, then malicious content status will
  // change to MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING.
  service->OnGaiaPasswordChanged();
  base::RunLoop().RunUntilIdle();
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(
    SecurityStateTabHelperTest,
    VerifyPasswordReuseMaliciousContentAndSecurityLevelWhenFeatureDisabled) {
  // Setup https server. This makes sure that the DANGEROUS security level is
  // not caused by any certificate error.
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      safe_browsing::kGoogleBrandedPhishingWarning);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  // Update security state of the current page to match
  // SB_THREAT_TYPE_PASSWORD_REUSE.
  safe_browsing::PasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  service->UpdateSecurityState(safe_browsing::SB_THREAT_TYPE_PASSWORD_REUSE,
                               contents);
  observer.WaitForDidChangeVisibleSecurityState();

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::SECURE, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);
}

// Tests that the security level of ftp: URLs is always downgraded to
// HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedOnFtpUrl) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("ftp://example.test/"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Ensure that WebContentsObservers don't show an incorrect Form Not Secure
  // explanation. Regression test for https://crbug.com/691412.
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(content::SSLStatus::NORMAL_CONTENT, entry->GetSSL().content_status);
}

const char kReportURI[] = "https://report-hpkp.test";

class PKPModelClientTest : public SecurityStateTabHelperTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_server_.Start());
    url_request_context_getter_ = browser()->profile()->GetRequestContext();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&PKPModelClientTest::SetUpOnIOThread,
                       base::Unretained(this)));
  }

  void SetUpOnIOThread() {
    net::URLRequestContext* request_context =
        url_request_context_getter_->GetURLRequestContext();
    net::TransportSecurityState* security_state =
        request_context->transport_security_state();

    base::Time expiration =
        base::Time::Now() + base::TimeDelta::FromSeconds(10000);

    net::HashValue hash(net::HASH_VALUE_SHA256);
    memset(hash.data(), 0x99, hash.size());
    net::HashValueVector hashes;
    hashes.push_back(hash);

    security_state->AddHPKP(https_server_.host_port_pair().host(), expiration,
                            true, hashes, GURL(kReportURI));
  }

 protected:
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
};

IN_PROC_BROWSER_TEST_F(PKPModelClientTest, PKPBypass) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
  net::CertVerifyResult verify_result;
  // PKP is bypassed when |is_issued_by_known_root| is false.
  verify_result.is_issued_by_known_root = false;
  verify_result.verified_cert = cert;
  net::HashValue hash(net::HASH_VALUE_SHA256);
  memset(hash.data(), 1, hash.size());
  verify_result.public_key_hashes.push_back(hash);

  mock_cert_verifier()->AddResultForCert(cert, verify_result, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, true,
      false);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();
  EXPECT_TRUE(explanation.pkp_bypassed);
  EXPECT_FALSE(explanation.info_explanations.empty());
}

IN_PROC_BROWSER_TEST_F(PKPModelClientTest, PKPEnforced) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
  net::CertVerifyResult verify_result;
  // PKP requires |is_issued_by_known_root| to be true.
  verify_result.is_issued_by_known_root = true;
  verify_result.verified_cert = cert;
  net::HashValue hash(net::HASH_VALUE_SHA256);
  memset(hash.data(), 1, hash.size());
  verify_result.public_key_hashes.push_back(hash);

  mock_cert_verifier()->AddResultForCert(cert, verify_result, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckBrokenSecurityStyle(observer, net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
                           browser(), cert.get());
}

// Fails requests with ERR_IO_PENDING. Can be used to simulate a navigation
// that never stops loading.
class PendingJobInterceptor : public net::URLRequestInterceptor {
 public:
  PendingJobInterceptor() {}
  ~PendingJobInterceptor() override {}

  // URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::ERR_IO_PENDING);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PendingJobInterceptor);
};

void InstallLoadingInterceptor(const std::string& host) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "http", host,
      std::unique_ptr<net::URLRequestInterceptor>(new PendingJobInterceptor()));
}

class SecurityStateLoadingTest : public SecurityStateTabHelperTest {
 public:
  SecurityStateLoadingTest() : SecurityStateTabHelperTest() {}
  ~SecurityStateLoadingTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&InstallLoadingInterceptor,
                       embedded_test_server()->GetURL("/title1.html").host()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateLoadingTest);
};

// Tests that navigation state changes cause the security state to be
// updated.
IN_PROC_BROWSER_TEST_F(SecurityStateLoadingTest, NavigationStateChanges) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::SECURE, false, security_state::CONTENT_STATUS_NONE, false,
      false /* expect cert status error */);

  // Navigate to a page that doesn't finish loading. Test that the
  // security state is neutral while the page is loading.
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/title1.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  CheckSecurityInfoForNonSecure(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Tests that when a visible password field is detected on an HTTP page
// load, the security level is downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngraded) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(), GetURLWithNonLocalHostname(embedded_test_server(),
                                            "/password/simple_password.html"));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected on a blob URL, the
// security level is downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngradedOnBlobUrl) {
  TestPasswordFieldOnBlobOrFilesystemURL(
      "blob",
      "var blob = new Blob(['<html><form><input type=password></form></html>'],"
      "                    {type: 'text/html'});"
      "window.domAutomationController.send(URL.createObjectURL(blob));",
      true /* expect_warning */);
}

// Tests that when no password field is detected on a blob URL, the security
// level is not downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnBlobUrl) {
  TestPasswordFieldOnBlobOrFilesystemURL(
      "blob",
      "var blob = new Blob(['<html>no password or credit card field</html>'],"
      "                    {type: 'text/html'});"
      "window.domAutomationController.send(URL.createObjectURL(blob));",
      false /* expect_warning */);
}

// Same as PasswordSecurityLevelDowngradedOnBlobUrl, but instead of a blob URL,
// this creates a filesystem URL.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngradedOnFilesystemUrl) {
  TestPasswordFieldOnBlobOrFilesystemURL(
      "filesystem",
      "window.webkitRequestFileSystem(window.TEMPORARY, 4096, function(fs) {"
      "  fs.root.getFile('test.html', {create: true}, function(fileEntry) {"
      "    fileEntry.createWriter(function(writer) {"
      "      writer.onwriteend = function(e) {"
      "        window.domAutomationController.send(fileEntry.toURL());"
      "      };"
      "      var blob ="
      "          new Blob(['<html><form><input type=password></form></html>'],"
      "                   {type: 'text/html'});"
      "      writer.write(blob);"
      "    });"
      "  });"
      "});",
      true /* expect_warning */);
}

// Same as DefaultSecurityLevelOnBlobUrl, but instead of a blob URL,
// this creates a filesystem URL.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       DefaultSecurityLevelOnFilesystemUrl) {
  TestPasswordFieldOnBlobOrFilesystemURL(
      "filesystem",
      "window.webkitRequestFileSystem(window.TEMPORARY, 4096, function(fs) {"
      "  fs.root.getFile('test.html', {create: true}, function(fileEntry) {"
      "    fileEntry.createWriter(function(writer) {"
      "      writer.onwriteend = function(e) {"
      "        window.domAutomationController.send(fileEntry.toURL());"
      "      };"
      "      var blob ="
      "          new Blob(['<html>no password or credit card field</html>'],"
      "                   {type: 'text/html'});"
      "      writer.write(blob);"
      "    });"
      "  });"
      "});",
      false /* expect_warning */);
}

// Tests that when an invisible password field is present on an HTTP page load,
// the security level is *not* downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelNotDowngradedForInvisibleInput) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/password/invisible_password.html"));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected inside an iframe on an
// HTTP page load, the security level is downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngradedFromIframe) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/password/simple_password_in_iframe.html"));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected inside an iframe on an
// HTTP page load, the security level is downgraded to HTTP_SHOW_WARNING, even
// if the iframe itself was loaded over HTTPS.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelDowngradedFromHttpsIframe) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP URL, which loads an iframe using the host and port of
  // |https_server_|.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/password/simple_password_in_https_iframe.html",
      https_server_.host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), replacement_path));
  InjectScript(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).password_field_shown);
}

// Tests that when a visible password field is detected on an HTTPS page load,
// the security level is *not* downgraded to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       PasswordSecurityLevelNotDowngradedOnHttps) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  GURL url = GetURLWithNonLocalHostname(&https_server_,
                                        "/password/simple_password.html");
  ui_test_utils::NavigateToURL(browser(), url);
  InjectScript(contents);
  // The security level should not be HTTP_SHOW_WARNING, because the page was
  // HTTPS instead of HTTP.
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::SECURE, security_info.security_level);

  // The SSLStatus flags should only be set if the top-level page load was HTTP,
  // which it was not in this case.
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(GetInputEvents(entry).password_field_shown);
}

// Tests that the security level of a HTTP page is downgraded to
// HTTP_SHOW_WARNING after editing a form field in the relevant configurations.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       SecurityLevelDowngradedAfterEditing) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/textinput/focus_input_on_load.html"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);

  // Type one character into the focused input control and wait for a security
  // state change.
  SecurityStyleTestObserver observer(contents);
  content::SimulateKeyPress(contents, ui::DomKey::FromCharacter('A'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  observer.WaitForDidChangeVisibleSecurityState();

  // Verify that the security state degrades as expected.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_TRUE(security_info.field_edit_downgraded_security_level);
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(GetInputEvents(entry).insecure_field_edited);

  {
    // Ensure that the security level remains Dangerous in the
    // kMarkHttpAsDangerous configuration.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        security_state::features::kMarkHttpAsFeature,
        {{security_state::features::kMarkHttpAsFeatureParameterName,
          security_state::features::kMarkHttpAsParameterDangerous}});

    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
    EXPECT_FALSE(security_info.field_edit_downgraded_security_level);
  }

  // Verify security state stays degraded after same-page navigation.
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithNonLocalHostname(
                     embedded_test_server(),
                     "/textinput/focus_input_on_load.html#fragment"));
  content::WaitForLoadStop(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_TRUE(security_info.field_edit_downgraded_security_level);
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());

  // Verify that after a refresh, the HTTP_SHOW_WARNING state is cleared.
  contents->GetController().Reload(content::ReloadType::NORMAL, false);
  content::WaitForLoadStop(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.field_edit_downgraded_security_level);
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
}

// Tests that the security level of a HTTP page is not downgraded when a form
// field is modified by JavaScript.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       SecurityLevelNotDowngradedAfterScriptModification) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/textinput/focus_input_on_load.html"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);

  // Verify a value set operation isn't treated as user-input.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('text_id').value='v';"));
  InjectScript(contents);
  base::RunLoop().RunUntilIdle();
  helper->GetSecurityInfo(&security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_FALSE(security_info.field_edit_downgraded_security_level);

  // Verify an InsertText operation isn't treated as user-input.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.execCommand('InsertText',false,'a');"));
  InjectScript(contents);
  base::RunLoop().RunUntilIdle();
  helper->GetSecurityInfo(&security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_FALSE(security_info.field_edit_downgraded_security_level);
}

// A Browser subclass that keeps track of messages that have been
// added to the console. Messages can be retrieved or cleared with
// console_messages() and ClearConsoleMessages(). The user of this class
// can set a callback to run when the next console message notification
// arrives.
class ConsoleWebContentsDelegate : public Browser {
 public:
  explicit ConsoleWebContentsDelegate(const Browser::CreateParams& params)
      : Browser(params) {}
  ~ConsoleWebContentsDelegate() override {}

  const std::vector<base::string16>& console_messages() const {
    return console_messages_;
  }

  void set_console_message_callback(const base::Closure& callback) {
    console_message_callback_ = callback;
  }

  void ClearConsoleMessages() { console_messages_.clear(); }

  // content::WebContentsDelegate
  bool DidAddMessageToConsole(content::WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override {
    console_messages_.push_back(message);
    if (!console_message_callback_.is_null()) {
      console_message_callback_.Run();
      console_message_callback_.Reset();
    }
    return true;
  }

 private:
  std::vector<base::string16> console_messages_;
  base::Closure console_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleWebContentsDelegate);
};

// Checks that |delegate| has observed exactly one console message for
// HTTP_SHOW_WARNING. To avoid brittleness, this just looks for keywords
// in the string rather than the exact text.
void CheckForOneHttpWarningConsoleMessage(
    ConsoleWebContentsDelegate* delegate) {
  const std::vector<base::string16>& messages = delegate->console_messages();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(base::string16::npos,
            messages[0].find(base::ASCIIToUTF16("warning has been added")));
}

// Tests that console messages are printed upon a call to
// GetSecurityInfo() on an HTTP_SHOW_WARNING page, exactly once per
// main-frame navigation.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, ConsoleMessage) {
  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  ASSERT_TRUE(contents);
  contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(contents, true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(contents, delegate->tab_strip_model()->GetActiveWebContents());

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());
  EXPECT_TRUE(delegate->console_messages().empty());

  // Trigger the HTTP_SHOW_WARNING state.
  base::RunLoop first_message;
  delegate->set_console_message_callback(first_message.QuitClosure());
  SimulatePasswordFieldShown(contents);
  first_message.Run();

  // Check that the HTTP_SHOW_WARNING state was actually triggered.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Two subsequent triggers of VisibleSecurityStateChanged -- one on the
  // same navigation and one on another navigation -- should only result
  // in one additional console message.
  SimulateCreditCardFieldEdit(contents);
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  SimulatePasswordFieldShown(contents);
  second_message.Run();

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that additional HTTP_SHOW_WARNING console messages are not
// printed after subframe navigations.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       ConsoleMessageNotPrintedForFrameNavigation) {
  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  ASSERT_TRUE(contents);
  contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(contents, true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(contents, delegate->tab_strip_model()->GetActiveWebContents());

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url = GetURLWithNonLocalHostname(embedded_test_server(),
                                             "/ssl/page_with_frame.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());
  EXPECT_TRUE(delegate->console_messages().empty());

  // Trigger the HTTP_SHOW_WARNING state.
  base::RunLoop first_message;
  delegate->set_console_message_callback(first_message.QuitClosure());
  SimulatePasswordFieldShown(contents);
  first_message.Run();

  // Check that the HTTP_SHOW_WARNING state was actually triggered.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Navigate the subframe and trigger VisibleSecurityStateChanged
  // again. While the security level is still HTTP_SHOW_WARNING, an
  // additional console message should not be logged because there was
  // already a console message logged for the current main-frame
  // navigation.
  content::WindowedNotificationObserver subframe_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &contents->GetController()));
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('navFrame').src = '/title2.html';"));
  subframe_observer.Wait();

  SimulateCreditCardFieldEdit(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Do a main frame navigation and then trigger HTTP_SHOW_WARNING
  // again. From the above subframe navigation and this main-frame
  // navigation, exactly one console message is expected.
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  SimulatePasswordFieldShown(contents);
  second_message.Run();

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that additional HTTP_SHOW_WARNING console messages are not
// printed after pushState navigations.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       ConsoleMessageNotPrintedForPushStateNavigation) {
  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  ASSERT_TRUE(contents);
  contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(contents, true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(contents, delegate->tab_strip_model()->GetActiveWebContents());

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());
  EXPECT_TRUE(delegate->console_messages().empty());

  // Trigger the HTTP_SHOW_WARNING state.
  base::RunLoop first_message;
  delegate->set_console_message_callback(first_message.QuitClosure());
  SimulatePasswordFieldShown(contents);
  first_message.Run();

  // Check that the HTTP_SHOW_WARNING state was actually triggered.
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Navigate with pushState and trigger VisibleSecurityStateChanged
  // again. While the security level is still HTTP_SHOW_WARNING, an
  // additional console message should not be logged because there was
  // already a console message logged for the current main-frame
  // navigation.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "history.pushState({ foo: 'bar' }, 'foo', 'bar');"));
  SimulateCreditCardFieldEdit(contents);
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Do a main frame navigation and then trigger HTTP_SHOW_WARNING
  // again. From the above pushState navigation and this main-frame
  // navigation, exactly one console message is expected.
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  SimulatePasswordFieldShown(contents);
  second_message.Run();

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that the security state for a WebContents is up to date when the
// WebContents is inserted into a Browser's TabStripModel.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, AddedTab) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  content::WebContents* new_contents = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  content::NavigationController& controller = new_contents->GetController();
  SecurityStateTabHelper::CreateForWebContents(new_contents);
  CheckSecurityInfoForNonSecure(new_contents);
  controller.LoadURL(https_server_.GetURL("/title1.html"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  CheckSecurityInfoForSecure(new_contents, security_state::SECURE, false,
                             security_state::CONTENT_STATUS_NONE, false,
                             false /* expect cert status error */);

  browser()->tab_strip_model()->InsertWebContentsAt(0, new_contents,
                                                    TabStripModel::ADD_NONE);
  CheckSecurityInfoForSecure(new_contents, security_state::SECURE, false,
                             security_state::CONTENT_STATUS_NONE, false,
                             false /* expect cert status error */);
}

// Tests that the WebContentsObserver::DidChangeVisibleSecurityState event fires
// with the current style on HTTP, broken HTTPS, and valid HTTPS pages.
IN_PROC_BROWSER_TEST_F(DidChangeVisibleSecurityStateTest,
                       DidChangeVisibleSecurityStateObserver) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  net::EmbeddedTestServer https_test_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_test_server_expired.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server_expired.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  // Visit an HTTP url.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().secure_explanations.size());
  EXPECT_FALSE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Visit an (otherwise valid) HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  GURL mixed_content_url(https_server_.GetURL(replacement_path));
  ui_test_utils::NavigateToURL(browser(), mixed_content_url);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  const content::SecurityStyleExplanations& mixed_content_explanation =
      observer.latest_explanations();
  ASSERT_EQ(1u, mixed_content_explanation.neutral_explanations.size());
  ASSERT_EQ(0u, mixed_content_explanation.insecure_explanations.size());
  ASSERT_EQ(2u, mixed_content_explanation.secure_explanations.size());
  CheckSecureCertificateExplanation(
      mixed_content_explanation.secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      mixed_content_explanation.secure_explanations[1], browser());
  EXPECT_TRUE(mixed_content_explanation.scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
  EXPECT_TRUE(mixed_content_explanation.displayed_mixed_content);
  EXPECT_FALSE(mixed_content_explanation.ran_mixed_content);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral,
            mixed_content_explanation.displayed_insecure_content_style);
  EXPECT_EQ(blink::kWebSecurityStyleInsecure,
            mixed_content_explanation.ran_insecure_content_style);

  // Visit a broken HTTPS url.
  GURL expired_url(https_test_server_expired.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), expired_url);

  // An interstitial should show, and an event for the lock icon on the
  // interstitial should fire.
  content::WaitForInterstitialAttach(web_contents);
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Before clicking through, navigate to a different page, and then go
  // back to the interstitial.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  ASSERT_EQ(3u, observer.latest_explanations().secure_explanations.size());
  CheckSecureCertificateExplanation(
      observer.latest_explanations().secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[1], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[2]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // After going back to the interstitial, an event for a broken lock
  // icon should fire again.
  ui_test_utils::NavigateToURL(browser(), expired_url);
  content::WaitForInterstitialAttach(web_contents);
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());

  // Since the next expected style is the same as the previous, clear
  // the observer (to make sure that the event fires twice and we don't
  // just see the previous event's style).
  observer.ClearLatestSecurityStyleAndExplanations();

  // Other conditions cannot be tested on this host after clicking
  // through because once the interstitial is clicked through, all URLs
  // for this host will remain in a broken state.
  ProceedThroughInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
}

// Tests that the security level of a HTTP page in Incognito mode is downgraded
// to HTTP_SHOW_WARNING.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperIncognitoTest,
                       SecurityLevelDowngradedForHTTPInIncognito) {
  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetBrowserContext()->IsOffTheRecord());
  contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(contents, true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(contents, delegate->tab_strip_model()->GetActiveWebContents());

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(http_url, entry->GetURL());

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));

  // Ensure that same-page pushstate does not add another notice.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "history.pushState({ foo: 'bar' }, 'foo', 'bar');"));
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  // Check that no additional console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
}

// Tests that additional HTTP_SHOW_WARNING console messages are not
// printed after aborted navigations.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperIncognitoTest,
                       ConsoleMessageNotPrintedForAbortedNavigation) {
  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(browser()->profile(), true));
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetBrowserContext()->IsOffTheRecord());
  contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(contents, true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(contents, delegate->tab_strip_model()->GetActiveWebContents());

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());

  // Check that the expected console message is present.
  ASSERT_NO_FATAL_FAILURE(CheckForOneHttpWarningConsoleMessage(delegate));
  delegate->ClearConsoleMessages();

  // Perform a navigation that does not commit.
  // The embedded test server returns a HTTP/204 only for local URLs, so
  // we cannot use GetURLWithNonLocalHostname() here.
  GURL http204_url = embedded_test_server()->GetURL("/nocontent");
  ui_test_utils::NavigateToURL(delegate, http204_url);

  // No change is expected in the security state.
  EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());
  EXPECT_EQ(1u, observer.latest_explanations().neutral_explanations.size());

  // No additional console logging should occur.
  EXPECT_TRUE(delegate->console_messages().empty());
}

// Tests that the security level of a HTTP page in Guest mode is not downgraded
// to HTTP_SHOW_WARNING.
#if defined(OS_CHROMEOS)
// Guest mode cannot be readily browser-tested on ChromeOS.
#define MAYBE_SecurityLevelNotDowngradedForHTTPInGuestMode \
  DISABLED_SecurityLevelNotDowngradedForHTTPInGuestMode
#else
#define MAYBE_SecurityLevelNotDowngradedForHTTPInGuestMode \
  SecurityLevelNotDowngradedForHTTPInGuestMode
#endif
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       MAYBE_SecurityLevelNotDowngradedForHTTPInGuestMode) {
  // Create a new browser in Guest Mode.
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());
  content::WindowedNotificationObserver browser_creation_observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  browser_creation_observer.Wait();
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  Browser* guest_browser = chrome::FindAnyBrowser(guest, true);
  ASSERT_TRUE(guest_browser);

  ConsoleWebContentsDelegate* delegate = new ConsoleWebContentsDelegate(
      Browser::CreateParams(guest_browser->profile(), true));
  content::WebContents* original_contents =
      guest_browser->tab_strip_model()->GetActiveWebContents();
  content::WebContents* contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          original_contents->GetBrowserContext()));
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetBrowserContext()->IsOffTheRecord());
  contents->SetDelegate(delegate);
  delegate->tab_strip_model()->AppendWebContents(contents, true);
  int index = delegate->tab_strip_model()->GetIndexOfWebContents(contents);
  delegate->tab_strip_model()->ActivateTabAt(index, true);
  ASSERT_EQ(contents, delegate->tab_strip_model()->GetActiveWebContents());

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(delegate, http_url);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, observer.latest_security_style());

  // No console notification should occur.
  EXPECT_TRUE(delegate->console_messages().empty());
}

// Tests that the security level of a HTTP page is downgraded to DANGEROUS when
// MarkHttpAsDangerous is enabled.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperIncognitoTest,
                       SecurityLevelDangerousWhenMarkHttpAsDangerous) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::kMarkHttpAsParameterDangerous}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->GetBrowserContext()->IsOffTheRecord());

  SecurityStyleTestObserver observer(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  GURL http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html");
  ui_test_utils::NavigateToURL(browser(), http_url);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  EXPECT_EQ(blink::kWebSecurityStyleInsecure, observer.latest_security_style());
}

// Visit a valid HTTPS page, then a broken HTTPS page, and then go back,
// and test that the observed security style matches.
#if defined(OS_CHROMEOS)
// Flaky on Chrome OS. See https://crbug.com/638576.
#define MAYBE_DidChangeVisibleSecurityStateObserverGoBack \
  DISABLED_DidChangeVisibleSecurityStateObserverGoBack
#else
#define MAYBE_DidChangeVisibleSecurityStateObserverGoBack \
  DidChangeVisibleSecurityStateObserverGoBack
#endif
IN_PROC_BROWSER_TEST_F(DidChangeVisibleSecurityStateTest,
                       MAYBE_DidChangeVisibleSecurityStateObserverGoBack) {
  ASSERT_TRUE(https_server_.Start());

  net::EmbeddedTestServer https_test_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_test_server_expired.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server_expired.Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  // Visit a valid HTTPS url.
  GURL valid_https_url(https_server_.GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  ASSERT_EQ(3u, observer.latest_explanations().secure_explanations.size());
  CheckSecureCertificateExplanation(
      observer.latest_explanations().secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[1], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[2]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);

  // Navigate to a bad HTTPS page on a different host, and then click
  // Back to verify that the previous good security style is seen again.
  GURL expired_https_url(https_test_server_expired.GetURL("/title1.html"));
  GURL::Replacements replace_host;
  replace_host.SetHostStr("www.example_broken.test");
  GURL https_url_different_host =
      expired_https_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), https_url_different_host);

  content::WaitForInterstitialAttach(web_contents);
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_COMMON_NAME_INVALID,
                           browser(),
                           https_test_server_expired.GetCertificate().get());
  ProceedThroughInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_COMMON_NAME_INVALID,
                           browser(),
                           https_test_server_expired.GetCertificate().get());
  ASSERT_EQ(2u, observer.latest_explanations().secure_explanations.size());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[0], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[1]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);

  content::WindowedNotificationObserver back_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  back_nav_load_observer.Wait();

  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());
  EXPECT_EQ(0u, observer.latest_explanations().neutral_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().insecure_explanations.size());
  ASSERT_EQ(3u, observer.latest_explanations().secure_explanations.size());
  CheckSecureCertificateExplanation(
      observer.latest_explanations().secure_explanations[0], browser(),
      https_server_.GetCertificate().get());
  CheckSecureConnectionExplanation(
      observer.latest_explanations().secure_explanations[1], browser());
  CheckSecureSubresourcesExplanation(
      observer.latest_explanations().secure_explanations[2]);
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
}

// After AddNonsecureUrlHandler() is called, requests to this hostname
// will use obsolete TLS settings.
const char kMockNonsecureHostname[] = "example-nonsecure.test";
const int kObsoleteTLSVersion = net::SSL_CONNECTION_VERSION_TLS1_1;
// ECDHE_RSA + AES_128_CBC with HMAC-SHA1
const uint16_t kObsoleteCipherSuite = 0xc013;

// A URLRequestMockHTTPJob that mocks a TLS connection with the obsolete
// TLS settings specified in kObsoleteTLSVersion and
// kObsoleteCipherSuite.
class URLRequestObsoleteTLSJob : public net::URLRequestMockHTTPJob {
 public:
  URLRequestObsoleteTLSJob(net::URLRequest* request,
                           net::NetworkDelegate* network_delegate,
                           const base::FilePath& file_path,
                           scoped_refptr<net::X509Certificate> cert)
      : net::URLRequestMockHTTPJob(request, network_delegate, file_path),
        cert_(std::move(cert)) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    net::URLRequestMockHTTPJob::GetResponseInfo(info);
    net::SSLConnectionStatusSetVersion(kObsoleteTLSVersion,
                                       &info->ssl_info.connection_status);
    net::SSLConnectionStatusSetCipherSuite(kObsoleteCipherSuite,
                                           &info->ssl_info.connection_status);
    info->ssl_info.cert = cert_;
  }

 protected:
  ~URLRequestObsoleteTLSJob() override {}

 private:
  const scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestObsoleteTLSJob);
};

// A URLRequestInterceptor that handles requests with
// URLRequestObsoleteTLSJob jobs.
class URLRequestNonsecureInterceptor : public net::URLRequestInterceptor {
 public:
  URLRequestNonsecureInterceptor(const base::FilePath& base_path,
                                 scoped_refptr<net::X509Certificate> cert)
      : base_path_(base_path), cert_(std::move(cert)) {}

  ~URLRequestNonsecureInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestObsoleteTLSJob(request, network_delegate, base_path_,
                                        cert_);
  }

 private:
  const base::FilePath base_path_;
  const scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestNonsecureInterceptor);
};

// Installs a handler to serve HTTPS requests to
// |kMockNonsecureHostname| with connections that have obsolete TLS
// settings.
void AddNonsecureUrlHandler(const base::FilePath& base_path,
                            scoped_refptr<net::X509Certificate> cert) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "https", kMockNonsecureHostname,
      std::unique_ptr<net::URLRequestInterceptor>(
          new URLRequestNonsecureInterceptor(base_path, cert)));
}

class BrowserTestNonsecureURLRequest : public InProcessBrowserTest {
 public:
  BrowserTestNonsecureURLRequest() : InProcessBrowserTest(), cert_(nullptr) {}

  void SetUpInProcessBrowserTestFixture() override {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);
  }

  void SetUpOnMainThread() override {
    base::FilePath serve_file;
    PathService::Get(chrome::DIR_TEST_DATA, &serve_file);
    serve_file = serve_file.Append(FILE_PATH_LITERAL("title1.html"));
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&AddNonsecureUrlHandler, serve_file, cert_));
  }

 private:
  scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTestNonsecureURLRequest);
};

// Tests that a connection with obsolete TLS settings does not get a
// secure connection explanation.
IN_PROC_BROWSER_TEST_F(
    BrowserTestNonsecureURLRequest,
    DidChangeVisibleSecurityStateObserverObsoleteTLSSettings) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  ui_test_utils::NavigateToURL(
      browser(), GURL(std::string("https://") + kMockNonsecureHostname));

  // The security style of the page doesn't get downgraded for obsolete
  // TLS settings, so it should remain at WebSecurityStyleSecure.
  EXPECT_EQ(blink::kWebSecurityStyleSecure, observer.latest_security_style());

  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  const char* protocol;
  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  net::SSLVersionToString(&protocol, ssl_version);
  for (const auto& explanation :
       observer.latest_explanations().secure_explanations) {
    EXPECT_NE(l10n_util::GetStringFUTF8(IDS_STRONG_SSL_SUMMARY,
                                        base::ASCIIToUTF16(protocol)),
              explanation.summary);
  }

  // Populate description string replacement with values corresponding
  // to test constants.
  std::vector<base::string16> description_replacements;
  description_replacements.push_back(base::ASCIIToUTF16("TLS 1.1"));
  description_replacements.push_back(
      l10n_util::GetStringUTF16(IDS_SSL_AN_OBSOLETE_PROTOCOL));
  description_replacements.push_back(base::ASCIIToUTF16("ECDHE_RSA"));
  description_replacements.push_back(
      l10n_util::GetStringUTF16(IDS_SSL_A_STRONG_KEY_EXCHANGE));
  description_replacements.push_back(
      base::ASCIIToUTF16("AES_128_CBC with HMAC-SHA1"));
  description_replacements.push_back(
      l10n_util::GetStringUTF16(IDS_SSL_AN_OBSOLETE_CIPHER));
  base::string16 obsolete_description = l10n_util::GetStringFUTF16(
      IDS_OBSOLETE_SSL_DESCRIPTION, description_replacements, nullptr);

  EXPECT_EQ(
      obsolete_description,
      base::ASCIIToUTF16(
          observer.latest_explanations().info_explanations[0].description));
}

// Tests that the Not Secure chip does not show for error pages on http:// URLs.
// Regression test for https://crbug.com/760647.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperIncognitoTest, HttpErrorPage) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to a URL that results in an error page. Even though the displayed
  // URL is http://, there shouldn't be a Not Secure warning because the browser
  // hasn't really navigated to an http:// page.
  ui_test_utils::NavigateToURL(browser(), GURL("http://nonexistent.test:17"));
  // Sanity-check that it is indeed an error page.
  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetController()
                                        .GetVisibleEntry();
  ASSERT_TRUE(entry);
  ASSERT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, MarkHttpAsWarning) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::kMarkHttpAsParameterWarning}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html"));

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       MarkHttpAsWarningAndDangerousOnFormEdits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnFormEdits}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(),
                                 "/textinput/focus_input_on_load.html"));

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Type one character into the focused input control and wait for a security
  // state change.
  SecurityStyleTestObserver observer(contents);
  content::SimulateKeyPress(contents, ui::DomKey::FromCharacter('A'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  observer.WaitForDidChangeVisibleSecurityState();

  // Verify that the security state degrades as expected.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       MarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}});

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  // Navigate to an HTTP page. Use a non-local hostname so that it is
  // not considered secure.
  ui_test_utils::NavigateToURL(
      browser(),
      GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html"));

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Insert a password field into the page and wait for a security state change.
  {
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(
        content::ExecuteScript(contents,
                               "var i = document.createElement('input');"
                               "i.type = 'password';"
                               "i.id = 'password-field';"
                               "document.body.appendChild(i);"));
    observer.WaitForDidChangeVisibleSecurityState();

    // Verify that the security state degrades as expected.
    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  }

  // Remove the password field and verify that the security level returns to
  // HTTP_SHOW_WARNING.
  {
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(content::ExecuteScript(contents,
                                       "document.body.removeChild(document."
                                       "getElementById('password-field'));"));
    observer.WaitForDidChangeVisibleSecurityState();
    helper->GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that the histogram for security level is recorded correctly for HTTP
// pages.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, HTTPSecurityLevelHistogram) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}});

  const char kHistogramName[] = "Security.SecurityLevel.NoncryptographicScheme";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    base::HistogramTester histograms;
    // Use a non-local hostname so that password fields (added below) downgrade
    // the security level.
    ui_test_utils::NavigateToURL(
        browser(),
        GetURLWithNonLocalHostname(embedded_test_server(), "/title1.html"));
    histograms.ExpectUniqueSample(kHistogramName,
                                  security_state::HTTP_SHOW_WARNING, 1);
  }

  // Add a password field and check that the histogram is recorded correctly.
  {
    base::HistogramTester histograms;
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(
        content::ExecuteScript(contents,
                               "var i = document.createElement('input');"
                               "i.type = 'password';"
                               "document.body.appendChild(i);"));
    observer.WaitForDidChangeVisibleSecurityState();
    histograms.ExpectUniqueSample(kHistogramName, security_state::DANGEROUS, 1);
  }
}

// Tests that the histogram for security level is recorded correctly for HTTPS
// pages.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest,
                       HTTPSSecurityLevelHistogram) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  const char kHistogramName[] = "Security.SecurityLevel.CryptographicScheme";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  {
    base::HistogramTester histograms;
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL("/title1.html"));
    histograms.ExpectUniqueSample(kHistogramName, security_state::SECURE, 1);
  }

  // Load mixed content and check that the histogram is recorded correctly.
  {
    base::HistogramTester histograms;
    SecurityStyleTestObserver observer(contents);
    EXPECT_TRUE(content::ExecuteScript(contents,
                                       "var i = document.createElement('img');"
                                       "i.src = 'http://example.test';"
                                       "document.body.appendChild(i);"));
    observer.WaitForDidChangeVisibleSecurityState();
    histograms.ExpectUniqueSample(kHistogramName, security_state::NONE, 1);
  }

  // Navigate away and the histogram should be recorded exactly once again, when
  // the new navigation commits.
  {
    base::HistogramTester histograms;
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL("/title2.html"));
    histograms.ExpectUniqueSample(kHistogramName, security_state::SECURE, 1);
  }
}

// Tests that the Certificate Transparency compliance of the main resource is
// recorded in a histogram.
IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, CTComplianceHistogram) {
  const char kHistogramName[] =
      "Security.CertificateTransparency.MainFrameNavigationCompliance";
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  histograms.ExpectUniqueSample(
      kHistogramName, net::ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS,
      1);
}

}  // namespace
