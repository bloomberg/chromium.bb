// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/ssl_status.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
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
#include "ui/base/l10n/l10n_util.h"

using security_state::SecurityStateModel;

namespace {

enum CertificateStatus { VALID_CERTIFICATE, INVALID_CERTIFICATE };

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

// A WebContentsObserver useful for testing the SecurityStyleChanged()
// method: it keeps track of the latest security style and explanation
// that was fired.
class SecurityStyleTestObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStyleTestObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        latest_security_style_(content::SECURITY_STYLE_UNKNOWN) {}
  ~SecurityStyleTestObserver() override {}

  void SecurityStyleChanged(content::SecurityStyle security_style,
                            const content::SecurityStyleExplanations&
                                security_style_explanations) override {
    latest_security_style_ = security_style;
    latest_explanations_ = security_style_explanations;
  }

  content::SecurityStyle latest_security_style() const {
    return latest_security_style_;
  }

  const content::SecurityStyleExplanations& latest_explanations() const {
    return latest_explanations_;
  }

  void ClearLatestSecurityStyleAndExplanations() {
    latest_security_style_ = content::SECURITY_STYLE_UNKNOWN;
    latest_explanations_ = content::SecurityStyleExplanations();
  }

 private:
  content::SecurityStyle latest_security_style_;
  content::SecurityStyleExplanations latest_explanations_;

  DISALLOW_COPY_AND_ASSIGN(SecurityStyleTestObserver);
};

// Check that |observer|'s latest event was for an expired certificate
// and that it saw the proper SecurityStyle and explanations.
void CheckBrokenSecurityStyle(const SecurityStyleTestObserver& observer,
                              int error,
                              Browser* browser) {
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            observer.latest_security_style());

  const content::SecurityStyleExplanations& expired_explanation =
      observer.latest_explanations();
  EXPECT_EQ(0u, expired_explanation.unauthenticated_explanations.size());
  ASSERT_EQ(1u, expired_explanation.broken_explanations.size());
  EXPECT_FALSE(expired_explanation.pkp_bypassed);

  // Check that the summary and description are as expected.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
            expired_explanation.broken_explanations[0].summary);

  base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(error));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
            expired_explanation.broken_explanations[0].description);

  // Check the associated certificate id.
  int cert_id = browser->tab_strip_model()
                    ->GetActiveWebContents()
                    ->GetController()
                    .GetActiveEntry()
                    ->GetSSL()
                    .cert_id;
  EXPECT_EQ(cert_id, expired_explanation.broken_explanations[0].cert_id);
}

// Checks that the given |secure_explanations| contains an appropriate
// explanation if the certificate status is valid.
void CheckSecureExplanations(
    const std::vector<content::SecurityStyleExplanation>& secure_explanations,
    CertificateStatus cert_status,
    Browser* browser) {
  ASSERT_EQ(cert_status == VALID_CERTIFICATE ? 2u : 1u,
            secure_explanations.size());
  if (cert_status == VALID_CERTIFICATE) {
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
              secure_explanations[0].summary);
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION),
        secure_explanations[0].description);
    int cert_id = browser->tab_strip_model()
                      ->GetActiveWebContents()
                      ->GetController()
                      .GetActiveEntry()
                      ->GetSSL()
                      .cert_id;
    EXPECT_EQ(cert_id, secure_explanations[0].cert_id);
  }

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURE_PROTOCOL_AND_CIPHERSUITE),
            secure_explanations.back().summary);
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_SECURE_PROTOCOL_AND_CIPHERSUITE_DESCRIPTION),
      secure_explanations.back().description);
}

void CheckSecurityInfoForSecure(
    content::WebContents* contents,
    SecurityStateModel::SecurityLevel expect_security_level,
    SecurityStateModel::SHA1DeprecationStatus expect_sha1_status,
    SecurityStateModel::MixedContentStatus expect_mixed_content_status,
    bool pkp_bypassed,
    bool expect_cert_error) {
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);
  const SecurityStateModel::SecurityInfo& security_info =
      model_client->GetSecurityInfo();
  EXPECT_EQ(expect_security_level, security_info.security_level);
  EXPECT_EQ(expect_sha1_status, security_info.sha1_deprecation_status);
  EXPECT_EQ(expect_mixed_content_status, security_info.mixed_content_status);
  EXPECT_TRUE(security_info.sct_verify_statuses.empty());
  EXPECT_TRUE(security_info.scheme_is_cryptographic);
  EXPECT_EQ(pkp_bypassed, security_info.pkp_bypassed);
  EXPECT_EQ(expect_cert_error,
            net::IsCertStatusError(security_info.cert_status));
  EXPECT_GT(security_info.security_bits, 0);

  content::CertStore* cert_store = content::CertStore::GetInstance();
  scoped_refptr<net::X509Certificate> cert;
  EXPECT_TRUE(cert_store->RetrieveCert(security_info.cert_id, &cert));
}

void CheckSecurityInfoForNonSecure(content::WebContents* contents) {
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);
  const SecurityStateModel::SecurityInfo& security_info =
      model_client->GetSecurityInfo();
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
  EXPECT_EQ(SecurityStateModel::NO_DEPRECATED_SHA1,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::NO_MIXED_CONTENT,
            security_info.mixed_content_status);
  EXPECT_TRUE(security_info.sct_verify_statuses.empty());
  EXPECT_FALSE(security_info.scheme_is_cryptographic);
  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(-1, security_info.security_bits);
  EXPECT_EQ(0, security_info.cert_id);
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

class ChromeSecurityStateModelClientTest : public CertVerifierBrowserTest {
 public:
  ChromeSecurityStateModelClientTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
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

    mock_cert_verifier()->AddResultForCert(cert.get(), verify_result,
                                           net_result);
  }

  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSecurityStateModelClientTest);
};

class SecurityStyleChangedTest : public InProcessBrowserTest {
 public:
  SecurityStyleChangedTest()
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
  DISALLOW_COPY_AND_ASSIGN(SecurityStyleChangedTest);
};

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, HttpPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);
  const SecurityStateModel::SecurityInfo& security_info =
      model_client->GetSecurityInfo();
  EXPECT_EQ(SecurityStateModel::NONE, security_info.security_level);
  EXPECT_EQ(SecurityStateModel::NO_DEPRECATED_SHA1,
            security_info.sha1_deprecation_status);
  EXPECT_EQ(SecurityStateModel::NO_MIXED_CONTENT,
            security_info.mixed_content_status);
  EXPECT_TRUE(security_info.sct_verify_statuses.empty());
  EXPECT_FALSE(security_info.scheme_is_cryptographic);
  EXPECT_FALSE(net::IsCertStatusError(security_info.cert_status));
  EXPECT_EQ(0, security_info.cert_id);
  EXPECT_EQ(-1, security_info.security_bits);
  EXPECT_EQ(0, security_info.connection_status);
}

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, HttpsPage) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, SHA1Broken) {
  ASSERT_TRUE(https_server_.Start());
  // The test server uses a long-lived cert by default, so a SHA1
  // signature in it will register as a "broken" condition rather than
  // "warning".
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, MixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);
  host_resolver()->AddRule("example.test",
                           https_server_.GetURL("/").host());

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::NONE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::NONE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::RAN_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content in an iframe.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/"));
  host_port_pair.set_host("different-host.test");
  host_resolver()->AddRule("different-host.test",
                           https_server_.GetURL("/").host());
  host_resolver()->AddRule("different-http-host.test",
                           embedded_test_server()->GetURL("/").host());
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe.html", host_port_pair,
      &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::RAN_MIXED_CONTENT, false,
      false /* expect cert status error */);
}

// Same as the test above but with a long-lived SHA1 cert.
IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest,
                       MixedContentWithBrokenSHA1) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  // The test server uses a long-lived cert by default, so a SHA1
  // signature in it will register as a "broken" condition rather than
  // "warning".
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT,
                                      net::OK);

  host_resolver()->AddRule("example.test",
                           https_server_.GetURL("/").host());

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      false /* expect cert status error */);
  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::RAN_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT, false,
      false /* expect cert status error */);
}

// Tests that the Content Security Policy block-all-mixed-content
// directive stops mixed content from running.
IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest,
                       MixedContentStrictBlocking) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page that tries to run mixed content in an
  // iframe, with strict mixed content blocking.
  std::string replacement_path;
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server_.GetURL("/"));
  host_port_pair.set_host("different-host.test");
  host_resolver()->AddRule("different-host.test",
                           https_server_.GetURL("/").host());
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content_in_iframe_with_strict_blocking.html",
      host_port_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, BrokenHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(net::CERT_STATUS_DATE_INVALID,
                                      net::ERR_CERT_DATE_INVALID);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      true /* expect cert status error */);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      true /* expect cert status error */);

  // Navigate to a broken HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT, false,
      true /* expect cert status error */);
}

const char kReportURI[] = "https://report-hpkp.test";

class PKPModelClientTest : public ChromeSecurityStateModelClientTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(https_server_.Start());
    url_request_context_getter_ = browser()->profile()->GetRequestContext();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&PKPModelClientTest::SetUpOnIOThread, this));
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

  mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, true, false);

  const content::SecurityStyleExplanations& explanation =
      observer.latest_explanations();
  EXPECT_TRUE(explanation.pkp_bypassed);
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

  mock_cert_verifier()->AddResultForCert(cert.get(), verify_result, net::OK);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckBrokenSecurityStyle(observer, net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
                           browser());
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

class SecurityStateModelLoadingTest
    : public ChromeSecurityStateModelClientTest {
 public:
  SecurityStateModelLoadingTest() : ChromeSecurityStateModelClientTest() {}
  ~SecurityStateModelLoadingTest() override{};

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstallLoadingInterceptor,
                   embedded_test_server()->GetURL("/").host()));
  }

  DISALLOW_COPY_AND_ASSIGN(SecurityStateModelLoadingTest);
};

// Tests that navigation state changes cause the security state to be
// updated.
IN_PROC_BROWSER_TEST_F(SecurityStateModelLoadingTest, NavigationStateChanges) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT, false,
      false /* expect cert status error */);

  // Navigate to a page that doesn't finish loading. Test that the
  // security state is neutral while the page is loading.
  browser()->OpenURL(content::OpenURLParams(embedded_test_server()->GetURL("/"),
                                            content::Referrer(), CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  CheckSecurityInfoForNonSecure(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Tests that the SecurityStateModel for a WebContents is up to date
// when the WebContents is inserted into a Browser's TabStripModel.
IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, AddedTab) {
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  content::WebContents* new_contents = content::WebContents::Create(
      content::WebContents::CreateParams(tab->GetBrowserContext()));
  content::NavigationController& controller = new_contents->GetController();
  ChromeSecurityStateModelClient::CreateForWebContents(new_contents);
  CheckSecurityInfoForNonSecure(new_contents);
  controller.LoadURL(https_server_.GetURL("/"), content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));
  CheckSecurityInfoForSecure(new_contents, SecurityStateModel::SECURE,
                             SecurityStateModel::NO_DEPRECATED_SHA1,
                             SecurityStateModel::NO_MIXED_CONTENT, false,
                             false /* expect cert status error */);

  browser()->tab_strip_model()->InsertWebContentsAt(0, new_contents,
                                                    TabStripModel::ADD_NONE);
  CheckSecurityInfoForSecure(new_contents, SecurityStateModel::SECURE,
                             SecurityStateModel::NO_DEPRECATED_SHA1,
                             SecurityStateModel::NO_MIXED_CONTENT, false,
                             false /* expect cert status error */);
}

// Tests that the WebContentsObserver::SecurityStyleChanged event fires
// with the current style on HTTP, broken HTTPS, and valid HTTPS pages.
IN_PROC_BROWSER_TEST_F(SecurityStyleChangedTest, SecurityStyleChangedObserver) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

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
  GURL http_url(embedded_test_server()->GetURL("/"));
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_EQ(content::SECURITY_STYLE_UNAUTHENTICATED,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().secure_explanations.size());
  EXPECT_FALSE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);

  // Visit an (otherwise valid) HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  GURL mixed_content_url(https_server_.GetURL(replacement_path));
  ui_test_utils::NavigateToURL(browser(), mixed_content_url);
  EXPECT_EQ(content::SECURITY_STYLE_UNAUTHENTICATED,
            observer.latest_security_style());

  const content::SecurityStyleExplanations& mixed_content_explanation =
      observer.latest_explanations();
  ASSERT_EQ(0u, mixed_content_explanation.unauthenticated_explanations.size());
  ASSERT_EQ(0u, mixed_content_explanation.broken_explanations.size());
  CheckSecureExplanations(mixed_content_explanation.secure_explanations,
                          VALID_CERTIFICATE, browser());
  EXPECT_TRUE(mixed_content_explanation.scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_TRUE(mixed_content_explanation.displayed_insecure_content);
  EXPECT_FALSE(mixed_content_explanation.ran_insecure_content);
  EXPECT_EQ(content::SECURITY_STYLE_UNAUTHENTICATED,
            mixed_content_explanation.displayed_insecure_content_style);
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            mixed_content_explanation.ran_insecure_content_style);

  // Visit a broken HTTPS url.
  GURL expired_url(https_test_server_expired.GetURL(std::string("/")));
  ui_test_utils::NavigateToURL(browser(), expired_url);

  // An interstitial should show, and an event for the lock icon on the
  // interstitial should fire.
  content::WaitForInterstitialAttach(web_contents);
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);

  // Before clicking through, navigate to a different page, and then go
  // back to the interstitial.
  GURL valid_https_url(https_server_.GetURL(std::string("/")));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATED,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          VALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);

  // After going back to the interstitial, an event for a broken lock
  // icon should fire again.
  ui_test_utils::NavigateToURL(browser(), expired_url);
  content::WaitForInterstitialAttach(web_contents);
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);

  // Since the next expected style is the same as the previous, clear
  // the observer (to make sure that the event fires twice and we don't
  // just see the previous event's style).
  observer.ClearLatestSecurityStyleAndExplanations();

  // Other conditions cannot be tested on this host after clicking
  // through because once the interstitial is clicked through, all URLs
  // for this host will remain in a broken state.
  ProceedThroughInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_DATE_INVALID, browser());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);
}

// Visit a valid HTTPS page, then a broken HTTPS page, and then go back,
// and test that the observed security style matches.
IN_PROC_BROWSER_TEST_F(SecurityStyleChangedTest,
                       SecurityStyleChangedObserverGoBack) {
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
  GURL valid_https_url(https_server_.GetURL(std::string("/")));
  ui_test_utils::NavigateToURL(browser(), valid_https_url);
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATED,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          VALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);

  // Navigate to a bad HTTPS page on a different host, and then click
  // Back to verify that the previous good security style is seen again.
  GURL expired_https_url(https_test_server_expired.GetURL(std::string("/")));
  host_resolver()->AddRule("www.example_broken.test", "127.0.0.1");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("www.example_broken.test");
  GURL https_url_different_host =
      expired_https_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), https_url_different_host);

  content::WaitForInterstitialAttach(web_contents);
  EXPECT_TRUE(web_contents->ShowingInterstitialPage());
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_COMMON_NAME_INVALID,
                           browser());
  ProceedThroughInterstitial(web_contents);
  CheckBrokenSecurityStyle(observer, net::ERR_CERT_COMMON_NAME_INVALID,
                           browser());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          INVALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);

  content::WindowedNotificationObserver back_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
  chrome::GoBack(browser(), CURRENT_TAB);
  back_nav_load_observer.Wait();

  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATED,
            observer.latest_security_style());
  EXPECT_EQ(0u,
            observer.latest_explanations().unauthenticated_explanations.size());
  EXPECT_EQ(0u, observer.latest_explanations().broken_explanations.size());
  CheckSecureExplanations(observer.latest_explanations().secure_explanations,
                          VALID_CERTIFICATE, browser());
  EXPECT_TRUE(observer.latest_explanations().scheme_is_cryptographic);
  EXPECT_FALSE(observer.latest_explanations().pkp_bypassed);
  EXPECT_FALSE(observer.latest_explanations().displayed_insecure_content);
  EXPECT_FALSE(observer.latest_explanations().ran_insecure_content);
}

// After AddNonsecureUrlHandler() is called, requests to this hostname
// will use obsolete TLS settings.
const char kMockNonsecureHostname[] = "example-nonsecure.test";

// A URLRequestMockHTTPJob that mocks a TLS connection with an obsolete
// protocol version.
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
    net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_1,
                                       &info->ssl_info.connection_status);
    const uint16_t kTlsEcdheRsaWithAes128CbcSha = 0xc013;
    net::SSLConnectionStatusSetCipherSuite(kTlsEcdheRsaWithAes128CbcSha,
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
                       SecurityStyleChangedObserverNonsecureConnection) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStyleTestObserver observer(web_contents);

  ui_test_utils::NavigateToURL(
      browser(), GURL(std::string("https://") + kMockNonsecureHostname));

  // The security style of the page doesn't get downgraded for obsolete
  // TLS settings, so it should remain at SECURITY_STYLE_AUTHENTICATED.
  EXPECT_EQ(content::SECURITY_STYLE_AUTHENTICATED,
            observer.latest_security_style());

  // The messages explaining the security style do, however, get
  // downgraded: SECURE_PROTOCOL_AND_CIPHERSUITE should not show up when
  // the TLS settings are obsolete.
  for (const auto& explanation :
       observer.latest_explanations().secure_explanations) {
    EXPECT_NE(l10n_util::GetStringUTF8(IDS_SECURE_PROTOCOL_AND_CIPHERSUITE),
              explanation.summary);
  }
}

}  // namespace
