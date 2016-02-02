// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_filter.h"

using security_state::SecurityStateModel;

namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

void CheckSecurityInfoForSecure(
    content::WebContents* contents,
    SecurityStateModel::SecurityLevel expect_security_level,
    SecurityStateModel::SHA1DeprecationStatus expect_sha1_status,
    SecurityStateModel::MixedContentStatus expect_mixed_content_status,
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

  static void GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    base::StringPairs replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    net::test_server::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
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
      SecurityStateModel::NO_MIXED_CONTENT,
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
      SecurityStateModel::NO_MIXED_CONTENT,
      false /* expect cert status error */);
}

IN_PROC_BROWSER_TEST_F(ChromeSecurityStateModelClientTest, MixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  SetUpMockCertVerifierForHttpsServer(0, net::OK);

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::NONE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURE, SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT,
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
      SecurityStateModel::DISPLAYED_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::RAN_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT,
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
      SecurityStateModel::RAN_MIXED_CONTENT,
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

  // Navigate to an HTTPS page that displays mixed content.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::DISPLAYED_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that displays mixed content dynamically.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::NO_MIXED_CONTENT,
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
      SecurityStateModel::DISPLAYED_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::RAN_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to an HTTPS page that runs and displays mixed content.
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_and_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));
  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::DEPRECATED_SHA1_MAJOR,
      SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT,
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
      SecurityStateModel::NO_MIXED_CONTENT,
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
      SecurityStateModel::NO_MIXED_CONTENT,
      true /* expect cert status error */);

  ProceedThroughInterstitial(
      browser()->tab_strip_model()->GetActiveWebContents());

  CheckSecurityInfoForSecure(
      browser()->tab_strip_model()->GetActiveWebContents(),
      SecurityStateModel::SECURITY_ERROR,
      SecurityStateModel::NO_DEPRECATED_SHA1,
      SecurityStateModel::NO_MIXED_CONTENT,
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
      SecurityStateModel::DISPLAYED_MIXED_CONTENT,
      true /* expect cert status error */);
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
      scoped_ptr<net::URLRequestInterceptor>(new PendingJobInterceptor()));
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
      SecurityStateModel::NO_MIXED_CONTENT,
      false /* expect cert status error */);

  // Navigate to a page that doesn't finish loading. Test that the
  // security state is neutral while the page is loading.
  browser()->OpenURL(content::OpenURLParams(embedded_test_server()->GetURL("/"),
                                            content::Referrer(), CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  CheckSecurityInfoForNonSecure(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Tests that the SecurityStateModel for a WebContents is up-to-date
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
                             SecurityStateModel::NO_MIXED_CONTENT,
                             false /* expect cert status error */);

  browser()->tab_strip_model()->InsertWebContentsAt(0, new_contents,
                                                    TabStripModel::ADD_NONE);
  CheckSecurityInfoForSecure(new_contents, SecurityStateModel::SECURE,
                             SecurityStateModel::NO_DEPRECATED_SHA1,
                             SecurityStateModel::NO_MIXED_CONTENT,
                             false /* expect cert status error */);
}

}  // namespace
