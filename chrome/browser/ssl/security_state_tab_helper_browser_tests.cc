// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_tab_helper.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/security_state/core/switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
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

// A WebContentsObserver useful for testing the DidChangeVisibleSecurityState()
// method: it keeps track of the latest security style and explanation that was
// fired.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        latest_security_style_(blink::WebSecurityStyleUnknown) {}
  ~SecurityStyleTestObserver() override {}

  void DidChangeVisibleSecurityState() override {
    content::SecurityStyleExplanations explanations;
    latest_security_style_ = web_contents()->GetDelegate()->GetSecurityStyle(
        web_contents(), &explanations);
    latest_explanations_ = explanations;
  }

  blink::WebSecurityStyle latest_security_style() const {
    return latest_security_style_;
  }

  const content::SecurityStyleExplanations& latest_explanations() const {
    return latest_explanations_;
  }

  void ClearLatestSecurityStyleAndExplanations() {
    latest_security_style_ = blink::WebSecurityStyleUnknown;
    latest_explanations_ = content::SecurityStyleExplanations();
  }

 private:
  blink::WebSecurityStyle latest_security_style_;
  content::SecurityStyleExplanations latest_explanations_;

  DISALLOW_COPY_AND_ASSIGN(SecurityStyleTestObserver);
};

// Check that |observer|'s latest event was for an expired certificate
// and that it saw the proper SecurityStyle and explanations.
void CheckBrokenSecurityStyle(const SecurityStyleTestObserver& observer,
                              int error,
                              Browser* browser,
                              net::X509Certificate* expected_cert) {
  EXPECT_EQ(blink::WebSecurityStyleAuthenticationBroken,
            observer.latest_security_style());

  const content::SecurityStyleExplanations& expired_explanation =
      observer.latest_explanations();
  EXPECT_EQ(0u, expired_explanation.unauthenticated_explanations.size());
  ASSERT_EQ(1u, expired_explanation.broken_explanations.size());
  EXPECT_FALSE(expired_explanation.pkp_bypassed);
  EXPECT_TRUE(expired_explanation.info_explanations.empty());

  // Check that the summary and description are as expected.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
            expired_explanation.broken_explanations[0].summary);

  base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(error));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
            expired_explanation.broken_explanations[0].description);

  // Check the associated certificate.
  net::X509Certificate* cert = browser->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetController()
                                   .GetActiveEntry()
                                   ->GetSSL()
                                   .certificate.get();
  EXPECT_TRUE(cert->Equals(expected_cert));
  EXPECT_TRUE(expired_explanation.broken_explanations[0].has_certificate);
}

// Checks that the given |secure_explanations| contains an appropriate
// explanation if the certificate status is valid.
void CheckSecureExplanations(
    const std::vector<content::SecurityStyleExplanation>& secure_explanations,
    CertificateStatus cert_status,
    Browser* browser,
    net::X509Certificate* expected_cert) {
  ASSERT_EQ(cert_status == VALID_CERTIFICATE ? 2u : 1u,
            secure_explanations.size());
  if (cert_status == VALID_CERTIFICATE) {
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
              secure_explanations[0].summary);
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION),
        secure_explanations[0].description);
    net::X509Certificate* cert = browser->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetController()
                                     .GetActiveEntry()
                                     ->GetSSL()
                                     .certificate.get();
    EXPECT_TRUE(cert->Equals(expected_cert));
    EXPECT_TRUE(secure_explanations[0].has_certificate);
  }

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_STRONG_SSL_SUMMARY),
            secure_explanations.back().summary);

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);

  const char *protocol, *key_exchange, *cipher, *mac;
  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  net::SSLVersionToString(&protocol, ssl_version);
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

  EXPECT_EQ(secure_description,
            base::ASCIIToUTF16(secure_explanations.back().description));
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
  EXPECT_TRUE(security_info.sct_verify_statuses.empty());
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
  EXPECT_TRUE(security_info.sct_verify_statuses.empty());
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
    host_resolver()->AddRule("*", embedded_test_server()->GetURL("/").host());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
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

    if (expect_warning) {
      EXPECT_EQ(security_state::HTTP_SHOW_WARNING,
                security_info.security_level);
      EXPECT_TRUE(entry->GetSSL().content_status &
                  content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
    } else {
      EXPECT_EQ(security_state::NONE, security_info.security_level);
      EXPECT_FALSE(entry->GetSSL().content_status &
                   content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
    }
  }

  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateTabHelperTest);
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
  EXPECT_TRUE(security_info.sct_verify_statuses.empty());
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
  ASSERT_EQ(1u, interstitial_explanation.broken_explanations.size());
  ASSERT_EQ(1u, interstitial_explanation.unauthenticated_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            interstitial_explanation.unauthenticated_explanations[0].summary);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      security_state::DANGEROUS, true, security_state::CONTENT_STATUS_NONE,
      false, true /* expect cert status error */);

  const content::SecurityStyleExplanations& page_explanation =
      observer.latest_explanations();
  ASSERT_EQ(1u, page_explanation.broken_explanations.size());
  ASSERT_EQ(1u, page_explanation.unauthenticated_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            page_explanation.unauthenticated_explanations[0].summary);
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

  ASSERT_EQ(0u, explanation.broken_explanations.size());
  ASSERT_EQ(1u, explanation.unauthenticated_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SHA1),
            explanation.unauthenticated_explanations[0].summary);
}

IN_PROC_BROWSER_TEST_F(SecurityStateTabHelperTest, MixedContent) {
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  host_resolver()->AddRule("example.test",
                           https_server_.GetURL("/title1.html").host());

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
  host_resolver()->AddRule("different-host.test",
                           https_server_.GetURL("/title1.html").host());
  host_resolver()->AddRule(
      "different-http-host.test",
      embedded_test_server()->GetURL("/title1.html").host());
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

  host_resolver()->AddRule("example.test",
                           https_server_.GetURL("/title1.html").host());

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
  host_resolver()->AddRule("different-host.test",
                           https_server_.GetURL("/title1.html").host());
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
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated,
            observer.latest_security_style());

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
        base::Bind(&PKPModelClientTest::SetUpOnIOThread,
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
  ~SecurityStateLoadingTest() override{};

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstallLoadingInterceptor,
                   embedded_test_server()->GetURL("/title1.html").host()));
  }

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
  EXPECT_TRUE(entry->GetSSL().content_status &
              content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
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
  EXPECT_FALSE(entry->GetSSL().content_status &
               content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
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
  EXPECT_TRUE(entry->GetSSL().content_status &
              content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
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
  EXPECT_TRUE(entry->GetSSL().content_status &
              content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
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
  EXPECT_FALSE(entry->GetSSL().content_status &
               content::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
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

  // Navigate to an HTTP page. Use a non-local hostname so that is it
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
  contents->OnPasswordInputShownOnHttp();
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
  contents->OnCreditCardInputShownOnHttp();
  GURL second_http_url =
      GetURLWithNonLocalHostname(embedded_test_server(), "/title2.html");
  ui_test_utils::NavigateToURL(delegate, second_http_url);
  entry = contents->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(second_http_url, entry->GetURL());

  base::RunLoop second_message;
  delegate->set_console_message_callback(second_message.QuitClosure());
  contents->OnPasswordInputShownOnHttp();
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

  // Navigate to an HTTP page. Use a non-local hostname so that is it
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
  contents->OnPasswordInputShownOnHttp();
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
  contents->OnCreditCardInputShownOnHttp();
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
  contents->OnPasswordInputShownOnHttp();
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

  // Navigate to an HTTP page. Use a non-local hostname so that is it
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
  contents->OnPasswordInputShownOnHttp();
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
  contents->OnCreditCardInputShownOnHttp();
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
  contents->OnPasswordInputShownOnHttp();
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
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
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
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated,
            observer.latest_security_style());

  const content::SecurityStyleExplanations& mixed_content_explanation =
      observer.latest_explanations();
  ASSERT_EQ(0u, mixed_content_explanation.unauthenticated_explanations.size());
  ASSERT_EQ(0u, mixed_content_explanation.broken_explanations.size());
  CheckSecureExplanations(mixed_content_explanation.secure_explanations,
                          VALID_CERTIFICATE, browser(),
                          https_server_.GetCertificate().get());
  EXPECT_TRUE(mixed_content_explanation.scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
  EXPECT_TRUE(mixed_content_explanation.displayed_mixed_content);
  EXPECT_FALSE(mixed_content_explanation.ran_mixed_content);
  EXPECT_EQ(blink::WebSecurityStyleUnauthenticated,
            mixed_content_explanation.displayed_insecure_content_style);
  EXPECT_EQ(blink::WebSecurityStyleAuthenticationBroken,
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
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser(),
                          https_test_server_expired.GetCertificate().get());
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
  EXPECT_EQ(blink::WebSecurityStyleAuthenticated,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          VALID_CERTIFICATE, browser(),
                          https_server_.GetCertificate().get());
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
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser(),
                          https_test_server_expired.GetCertificate().get());
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
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser(),
                          https_test_server_expired.GetCertificate().get());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);
  EXPECT_TRUE(observer.latest_explanations().summary.empty());
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
  EXPECT_EQ(blink::WebSecurityStyleAuthenticated,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          VALID_CERTIFICATE, browser(),
                          https_server_.GetCertificate().get());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(observer.latest_explanations().info_explanations.empty());
  EXPECT_FALSE(observer.latest_explanations().displayed_mixed_content);
  EXPECT_FALSE(observer.latest_explanations().ran_mixed_content);

  // Navigate to a bad HTTPS page on a different host, and then click
  // Back to verify that the previous good security style is seen again.
  GURL expired_https_url(https_test_server_expired.GetURL("/title1.html"));
  host_resolver()->AddRule("www.example_broken.test", "127.0.0.1");
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
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser(),
                          https_test_server_expired.GetCertificate().get());
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

  EXPECT_EQ(blink::WebSecurityStyleAuthenticated,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          VALID_CERTIFICATE, browser(),
                          https_server_.GetCertificate().get());
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
                           scoped_refptr<net::X509Certificate> cert,
                           scoped_refptr<base::TaskRunner> task_runner)
      : net::URLRequestMockHTTPJob(request,
                                   network_delegate,
                                   file_path,
                                   task_runner),
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
  URLRequestNonsecureInterceptor(
      const base::FilePath& base_path,
      scoped_refptr<base::SequencedWorkerPool> worker_pool,
      scoped_refptr<net::X509Certificate> cert)
      : base_path_(base_path),
        worker_pool_(std::move(worker_pool)),
        cert_(std::move(cert)) {}

  ~URLRequestNonsecureInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestObsoleteTLSJob(
        request, network_delegate, base_path_, cert_,
        worker_pool_->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  }

 private:
  const base::FilePath base_path_;
  const scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  const scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestNonsecureInterceptor);
};

// Installs a handler to serve HTTPS requests to
// |kMockNonsecureHostname| with connections that have obsolete TLS
// settings.
void AddNonsecureUrlHandler(
    const base::FilePath& base_path,
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<base::SequencedWorkerPool> worker_pool) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "https", kMockNonsecureHostname,
      std::unique_ptr<net::URLRequestInterceptor>(
          new URLRequestNonsecureInterceptor(base_path, worker_pool, cert)));
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
        base::Bind(
            &AddNonsecureUrlHandler, serve_file, cert_,
            make_scoped_refptr(content::BrowserThread::GetBlockingPool())));
  }

 private:
  scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTestNonsecureURLRequest);
};

// Tests that a connection with obsolete TLS settings does not get a
// secure connection explanation.
IN_PROC_BROWSER_TEST_F(BrowserTestNonsecureURLRequest,
                       DidChangeVisibleSecurityStateObserverNonsecureConnection) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  ui_test_utils::NavigateToURL(
      browser(), GURL(std::string("https://") + kMockNonsecureHostname));

  // The security style of the page doesn't get downgraded for obsolete
  // TLS settings, so it should remain at WebSecurityStyleAuthenticated.
  EXPECT_EQ(blink::WebSecurityStyleAuthenticated,
            observer.latest_security_style());

  // The messages explaining the security style do, however, get
  // downgraded: SECURE_PROTOCOL_AND_CIPHERSUITE should not show up when
  // the TLS settings are obsolete.
  for (const auto& explanation :
       observer.latest_explanations().secure_explanations) {
    EXPECT_NE(l10n_util::GetStringUTF8(IDS_STRONG_SSL_SUMMARY),
              explanation.summary);
  }

  // Populate description string replacement with values corresponding
  // to test constants.
  std::vector<base::string16> description_replacements;
  description_replacements.push_back(
      l10n_util::GetStringUTF16(IDS_SSL_AN_OBSOLETE_PROTOCOL));
  description_replacements.push_back(base::ASCIIToUTF16("TLS 1.1"));
  description_replacements.push_back(
      l10n_util::GetStringUTF16(IDS_SSL_A_STRONG_KEY_EXCHANGE));
  description_replacements.push_back(base::ASCIIToUTF16("ECDHE_RSA"));
  description_replacements.push_back(
      l10n_util::GetStringUTF16(IDS_SSL_AN_OBSOLETE_CIPHER));
  description_replacements.push_back(
      base::ASCIIToUTF16("AES_128_CBC with HMAC-SHA1"));
  base::string16 obsolete_description = l10n_util::GetStringFUTF16(
      IDS_OBSOLETE_SSL_DESCRIPTION, description_replacements, nullptr);

  EXPECT_EQ(
      obsolete_description,
      base::ASCIIToUTF16(
          observer.latest_explanations().info_explanations[0].description));
}

// After AddSCTUrlHandler() is called, requests to this hostname
// will be served with Signed Certificate Timestamps.
const char kMockHostnameWithSCTs[] = "example-scts.test";

// URLRequestJobWithSCTs mocks a connection that includes a set of dummy
// SCTs with these statuses.
const std::vector<net::ct::SCTVerifyStatus> kTestSCTStatuses{
    net::ct::SCT_STATUS_OK, net::ct::SCT_STATUS_LOG_UNKNOWN,
    net::ct::SCT_STATUS_OK};

// A URLRequestMockHTTPJob that mocks a TLS connection with SCTs
// attached to it. The SCTs will have verification statuses
// |kTestSCTStatuses|.
class URLRequestJobWithSCTs : public net::URLRequestMockHTTPJob {
 public:
  URLRequestJobWithSCTs(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        const base::FilePath& file_path,
                        scoped_refptr<net::X509Certificate> cert,
                        scoped_refptr<base::TaskRunner> task_runner)
      : net::URLRequestMockHTTPJob(request,
                                   network_delegate,
                                   file_path,
                                   task_runner),
        cert_(std::move(cert)) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    net::URLRequestMockHTTPJob::GetResponseInfo(info);
    for (const auto& status : kTestSCTStatuses) {
      scoped_refptr<net::ct::SignedCertificateTimestamp> dummy_sct =
          new net::ct::SignedCertificateTimestamp();
      info->ssl_info.signed_certificate_timestamps.push_back(
          net::SignedCertificateTimestampAndStatus(dummy_sct, status));
    }
    info->ssl_info.cert = cert_;
  }

 protected:
  ~URLRequestJobWithSCTs() override {}

 private:
  const scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestJobWithSCTs);
};

// A URLRequestInterceptor that handles requests with
// URLRequestJobWithSCTs jobs.
class URLRequestWithSCTsInterceptor : public net::URLRequestInterceptor {
 public:
  URLRequestWithSCTsInterceptor(
      const base::FilePath& base_path,
      scoped_refptr<base::SequencedWorkerPool> worker_pool,
      scoped_refptr<net::X509Certificate> cert)
      : base_path_(base_path),
        worker_pool_(std::move(worker_pool)),
        cert_(std::move(cert)) {}

  ~URLRequestWithSCTsInterceptor() override {}

  // net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new URLRequestJobWithSCTs(
        request, network_delegate, base_path_, cert_,
        worker_pool_->GetTaskRunnerWithShutdownBehavior(
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  }

 private:
  const base::FilePath base_path_;
  const scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  const scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestWithSCTsInterceptor);
};

// Installs a handler to serve HTTPS requests to |kMockHostnameWithSCTs|
// with connections that have SCTs.
void AddSCTUrlHandler(const base::FilePath& base_path,
                      scoped_refptr<net::X509Certificate> cert,
                      scoped_refptr<base::SequencedWorkerPool> worker_pool) {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddHostnameInterceptor(
      "https", kMockHostnameWithSCTs,
      std::unique_ptr<net::URLRequestInterceptor>(
          new URLRequestWithSCTsInterceptor(base_path, worker_pool, cert)));
}

class BrowserTestURLRequestWithSCTs : public InProcessBrowserTest {
 public:
  BrowserTestURLRequestWithSCTs() : InProcessBrowserTest(), cert_(nullptr) {}

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
        base::Bind(
            &AddSCTUrlHandler, serve_file, cert_,
            make_scoped_refptr(content::BrowserThread::GetBlockingPool())));
  }

 private:
  scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTestURLRequestWithSCTs);
};

// Tests that, when Signed Certificate Timestamps (SCTs) are served on a
// connection, the SCTs verification statuses are exposed on the
// SecurityInfo.
IN_PROC_BROWSER_TEST_F(BrowserTestURLRequestWithSCTs,
                       SecurityInfoWithSCTsAttached) {
  ui_test_utils::NavigateToURL(
      browser(), GURL(std::string("https://") + kMockHostnameWithSCTs));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::SECURE, security_info.security_level);
  EXPECT_EQ(kTestSCTStatuses, security_info.sct_verify_statuses);
}

}  // namespace
