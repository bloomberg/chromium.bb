// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/browser/ssl/mitm_software_blocking_page.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/insecure_content_renderer.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/security_state/core/switches.h"
#include "components/ssl_config/ssl_config_prefs.h"
#include "components/ssl_errors/error_classification.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_state.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#endif

#if defined(USE_NSS_CERTS)
#include "chrome/browser/net/nss_context.h"
#include "net/cert/nss_cert_database.h"
#endif  // defined(USE_NSS_CERTS)

using base::ASCIIToUTF16;
using chrome_browser_interstitials::SecurityInterstitialIDNTest;
using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;
using security_interstitials::SecurityInterstitialControllerClient;
using web_modal::WebContentsModalDialogManager;

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

const uint32_t kLargeVersionId = 0xFFFFFFu;

const char kHstsTestHostName[] = "hsts-example.test";

namespace {

enum ProceedDecision {
  SSL_INTERSTITIAL_PROCEED,
  SSL_INTERSTITIAL_DO_NOT_PROCEED
};

namespace AuthState {

enum AuthStateFlags {
  NONE = 0,
  DISPLAYED_INSECURE_CONTENT = 1 << 0,
  RAN_INSECURE_CONTENT = 1 << 1,
  SHOWING_INTERSTITIAL = 1 << 2,
  SHOWING_ERROR = 1 << 3,
  DISPLAYED_FORM_WITH_INSECURE_ACTION = 1 << 4
};

void Check(const NavigationEntry& entry, int expected_authentication_state) {
  if (expected_authentication_state == AuthState::SHOWING_ERROR) {
    EXPECT_EQ(content::PAGE_TYPE_ERROR, entry.GetPageType());
  } else {
    EXPECT_EQ(
        !!(expected_authentication_state & AuthState::SHOWING_INTERSTITIAL)
            ? content::PAGE_TYPE_INTERSTITIAL
            : content::PAGE_TYPE_NORMAL,
        entry.GetPageType());
  }

  bool displayed_insecure_content =
      !!(entry.GetSSL().content_status & SSLStatus::DISPLAYED_INSECURE_CONTENT);
  EXPECT_EQ(
      !!(expected_authentication_state & AuthState::DISPLAYED_INSECURE_CONTENT),
      displayed_insecure_content);

  bool ran_insecure_content =
      !!(entry.GetSSL().content_status & SSLStatus::RAN_INSECURE_CONTENT);
  EXPECT_EQ(!!(expected_authentication_state & AuthState::RAN_INSECURE_CONTENT),
            ran_insecure_content);

  bool displayed_form_with_insecure_action =
      !!(entry.GetSSL().content_status &
         SSLStatus::DISPLAYED_FORM_WITH_INSECURE_ACTION);
  EXPECT_EQ(!!(expected_authentication_state &
               AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION),
            displayed_form_with_insecure_action);
}

}  // namespace AuthState

namespace SecurityStyle {

void Check(WebContents* tab,
           security_state::SecurityLevel expected_security_level) {
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(expected_security_level, security_info.security_level);
}

}  // namespace SecurityStyle

namespace CertError {

enum CertErrorFlags {
  NONE = 0
};

void Check(const NavigationEntry& entry, net::CertStatus error) {
  if (error) {
    EXPECT_EQ(error, entry.GetSSL().cert_status & error);
    net::CertStatus extra_cert_errors =
        error ^ (entry.GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
    EXPECT_FALSE(extra_cert_errors) << "Got unexpected cert error: "
                                    << extra_cert_errors;
  } else {
    EXPECT_EQ(0U, entry.GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
  }
}

}  // namespace CertError

void CheckSecurityState(WebContents* tab,
                        net::CertStatus expected_error,
                        security_state::SecurityLevel expected_security_level,
                        int expected_authentication_state) {
  ASSERT_FALSE(tab->IsCrashed());
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  CertError::Check(*entry, expected_error);
  SecurityStyle::Check(tab, expected_security_level);
  AuthState::Check(*entry, expected_authentication_state);
}

// This observer waits for the SSLErrorHandler to start an interstitial timer
// for the given web contents.
class SSLInterstitialTimerObserver {
 public:
  explicit SSLInterstitialTimerObserver(content::WebContents* web_contents)
      : web_contents_(web_contents), message_loop_runner_(new base::RunLoop) {
    callback_ = base::Bind(&SSLInterstitialTimerObserver::OnTimerStarted,
                           base::Unretained(this));
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(&callback_);
  }

  ~SSLInterstitialTimerObserver() {
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(nullptr);
  }

  // Waits until the interstitial delay timer in SSLErrorHandler is started.
  void WaitForTimerStarted() { message_loop_runner_->Run(); }

  // Returns true if the interstitial delay timer has been started.
  bool timer_started() const { return timer_started_; }

 private:
  void OnTimerStarted(content::WebContents* web_contents) {
    timer_started_ = true;
    if (web_contents_ == web_contents)
      message_loop_runner_->Quit();
  }

  bool timer_started_ = false;
  const content::WebContents* web_contents_;
  SSLErrorHandler::TimerStartedCallback callback_;

  std::unique_ptr<base::RunLoop> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLInterstitialTimerObserver);
};

class HungJob : public net::URLRequestJob {
 public:
  HungJob(net::URLRequest* request, net::NetworkDelegate* network_delegate)
      : net::URLRequestJob(request, network_delegate) {}

  void Start() override {
  }
};

class FaviconFilter : public net::URLRequestInterceptor {
 public:
  FaviconFilter() {}
  ~FaviconFilter() override {}

  // net::URLRequestInterceptor implementation
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    if (request->url().path() == "/favicon.ico")
      return new HungJob(request, network_delegate);
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconFilter);
};

class ChromeContentBrowserClientForMixedContentTest
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientForMixedContentTest() {}
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* web_prefs) override {
    web_prefs->allow_running_insecure_content = allow_running_insecure_content_;
    web_prefs->strict_mixed_content_checking = strict_mixed_content_checking_;
    web_prefs->strictly_block_blockable_mixed_content =
        strictly_block_blockable_mixed_content_;
  }
  void SetMixedContentSettings(bool allow_running_insecure_content,
                               bool strict_mixed_content_checking,
                               bool strictly_block_blockable_mixed_content) {
    allow_running_insecure_content_ = allow_running_insecure_content;
    strict_mixed_content_checking_ = strict_mixed_content_checking;
    strictly_block_blockable_mixed_content_ =
        strictly_block_blockable_mixed_content;
  }

 private:
  bool allow_running_insecure_content_ = false;
  bool strict_mixed_content_checking_ = false;
  bool strictly_block_blockable_mixed_content_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientForMixedContentTest);
};

std::string EncodeQuery(const std::string& query) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(query.data(), query.size(), &buffer);
  return std::string(buffer.data(), buffer.length());
}

// Returns the Sha256 hash of the SPKI of |cert|.
net::HashValue GetSPKIHash(net::X509Certificate* cert) {
  std::string der_data;
  EXPECT_TRUE(
      net::X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_data));
  base::StringPiece der_bytes(der_data);
  base::StringPiece spki_bytes;
  EXPECT_TRUE(net::asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes));
  net::HashValue sha256(net::HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
  return sha256;
}

net::SpawnedTestServer::SSLOptions GetOCSPSSLOptions(
    net::SpawnedTestServer::SSLOptions::OCSPStatus ocsp_status) {
  net::SpawnedTestServer::SSLOptions ssl_options(
      net::SpawnedTestServer::SSLOptions::CERT_AUTO);
  ssl_options.ocsp_status = ocsp_status;
  return ssl_options;
}

// Compares two SSLStatuses to check if they match up before and after an
// interstitial. To match up, they should have the same connection information
// properties, such as certificate, connection status, connection security,
// etc. Content status and user data are not compared. Returns true if the
// statuses match and false otherwise.
bool ComparePreAndPostInterstitialSSLStatuses(const content::SSLStatus& one,
                                              const content::SSLStatus& two) {
  return one.initialized == two.initialized &&
         !!one.certificate == !!two.certificate &&
         (one.certificate ? one.certificate->Equals(two.certificate.get())
                          : true) &&
         one.cert_status == two.cert_status &&
         one.security_bits == two.security_bits &&
         one.key_exchange_group == two.key_exchange_group &&
         one.connection_status == two.connection_status &&
         one.pkp_bypassed == two.pkp_bypassed;
}

// Set HSTS for the test host name, so that all errors thrown on this domain
// will be nonoverridable.
void SetHSTSForHostName(
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
  net::TransportSecurityState* state =
      context_getter->GetURLRequestContext()->transport_security_state();
  const base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  EXPECT_FALSE(state->ShouldUpgradeToSSL(kHstsTestHostName));
  state->AddHSTS(kHstsTestHostName, expiry, false);
  EXPECT_TRUE(state->ShouldUpgradeToSSL(kHstsTestHostName));
}

}  // namespace

class SSLUITest : public InProcessBrowserTest {
 public:
  SSLUITest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_ocsp_ok_(
            net::SpawnedTestServer::TYPE_HTTPS,
            GetOCSPSSLOptions(net::SpawnedTestServer::SSLOptions::OCSP_OK),
            base::FilePath(kDocRoot)),
        https_server_ocsp_revoked_(
            net::SpawnedTestServer::TYPE_HTTPS,
            GetOCSPSSLOptions(net::SpawnedTestServer::SSLOptions::OCSP_REVOKED),
            base::FilePath(kDocRoot)),
        wss_server_expired_(net::SpawnedTestServer::TYPE_WSS,
                            SSLOptions(SSLOptions::CERT_EXPIRED),
                            net::GetWebSocketTestDataDirectory()),
        wss_server_mismatched_(net::SpawnedTestServer::TYPE_WSS,
                               SSLOptions(SSLOptions::CERT_MISMATCHED_NAME),
                               net::GetWebSocketTestDataDirectory()) {
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));

    https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    https_server_expired_.AddDefaultHandlers(base::FilePath(kDocRoot));

    https_server_mismatched_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_.AddDefaultHandlers(base::FilePath(kDocRoot));

    // Sometimes favicons load before tests check the authentication
    // state, and sometimes they load after. This is problematic on
    // tests that load pages with certificate errors, because the page
    // will be marked as having displayed subresources with certificate
    // errors only if the favicon loads before the test checks the
    // authentication state. To avoid this non-determinism, add an
    // interceptor to hang all favicon requests.
    std::unique_ptr<net::URLRequestInterceptor> interceptor(new FaviconFilter);
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "https", "127.0.0.1", std::move(interceptor));
    interceptor.reset(new FaviconFilter);
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "https", "localhost", std::move(interceptor));
  }

  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
  }

  void TearDown() override {
    SSLErrorHandler::ResetConfigForTesting();
    InProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    // Use process-per-site so that navigating to a same-site page in a
    // new tab will use the same process.
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void CheckAuthenticatedState(WebContents* tab,
                               int expected_authentication_state) {
    CheckSecurityState(tab, CertError::NONE, security_state::SECURE,
                       expected_authentication_state);
  }

  void CheckUnauthenticatedState(WebContents* tab,
                                 int expected_authentication_state) {
    CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                       expected_authentication_state);
  }

  void CheckAuthenticationBrokenState(WebContents* tab,
                                      net::CertStatus error,
                                      int expected_authentication_state) {
    CheckSecurityState(tab, error, security_state::DANGEROUS,
                       expected_authentication_state);
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION doesn't lower the security level
    // to DANGEROUS.
    ASSERT_NE(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION, error);
  }

  void ProceedThroughInterstitial(WebContents* tab) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    interstitial_page->Proceed();
    observer.Wait();
  }

  void SendInterstitialCommand(WebContents* tab, std::string command) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);
    ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    SSLBlockingPage* ssl_interstitial = static_cast<SSLBlockingPage*>(
        interstitial_page->GetDelegateForTesting());
    ssl_interstitial->CommandReceived(command);
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

  static void GetTopFramePath(const net::EmbeddedTestServer& http_server,
                              const net::EmbeddedTestServer& good_https_server,
                              const net::EmbeddedTestServer& bad_https_server,
                              std::string* top_frame_path) {
    // The "frame_left.html" page contained in the top_frame.html page contains
    // <a href>'s to three different servers. This sets up all of the
    // replacement text to work with test servers which listen on ephemeral
    // ports.
    GURL http_url = http_server.GetURL("/ssl/google.html");
    GURL good_https_url = good_https_server.GetURL("/ssl/google.html");
    GURL bad_https_url = bad_https_server.GetURL("/ssl/bad_iframe.html");

    base::StringPairs replacement_text_frame_left;
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_HTTP_PORT", http_url.port()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_GOOD_HTTPS_PAGE", good_https_url.spec()));
    replacement_text_frame_left.push_back(
        make_pair("REPLACE_WITH_BAD_HTTPS_PAGE", bad_https_url.spec()));
    std::string frame_left_path;
    net::test_server::GetFilePathWithReplacements(
        "frame_left.html", replacement_text_frame_left, &frame_left_path);

    // Substitute the generated frame_left URL into the top_frame page.
    base::StringPairs replacement_text_top_frame;
    replacement_text_top_frame.push_back(
        make_pair("REPLACE_WITH_FRAME_LEFT_PATH", frame_left_path));
    net::test_server::GetFilePathWithReplacements(
        "/ssl/top_frame.html", replacement_text_top_frame, top_frame_path);
  }

  // Helper function for testing invalid certificate chain reporting.
  void TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::OptIn opt_in,
      ProceedDecision proceed,
      certificate_reporting_test_utils::ExpectReport expect_report,
      Browser* browser) {
    ASSERT_TRUE(https_server_expired_.Start());

    base::RunLoop run_loop;
    certificate_reporting_test_utils::SSLCertReporterCallback reporter_callback(
        &run_loop);

    // Opt in to sending reports for invalid certificate chains.
    certificate_reporting_test_utils::SetCertReportingOptIn(browser, opt_in);

    ui_test_utils::NavigateToURL(browser,
                                 https_server_expired_.GetURL("/title1.html"));

    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(tab != nullptr);
    CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                   AuthState::SHOWING_INTERSTITIAL);

    std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
        certificate_reporting_test_utils::CreateMockSSLCertReporter(
            base::Bind(&certificate_reporting_test_utils::
                           SSLCertReporterCallback::ReportSent,
                       base::Unretained(&reporter_callback)),
            expect_report);

    ASSERT_TRUE(tab->GetInterstitialPage() != nullptr);
    SSLBlockingPage* interstitial_page = static_cast<SSLBlockingPage*>(
        tab->GetInterstitialPage()->GetDelegateForTesting());
    ASSERT_TRUE(interstitial_page != nullptr);
    interstitial_page->SetSSLCertReporterForTesting(
        std::move(ssl_cert_reporter));

    EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
    EXPECT_EQ(certificate_reporting::CertLoggerRequest::CHROME_CHANNEL_NONE,
              reporter_callback.GetLatestChromeChannelReported());

    // Leave the interstitial (either by proceeding or going back)
    if (proceed == SSL_INTERSTITIAL_PROCEED) {
      ProceedThroughInterstitial(tab);
    } else {
      // Click "Take me back"
      InterstitialPage* interstitial_page = tab->GetInterstitialPage();
      ASSERT_TRUE(interstitial_page);
      interstitial_page->DontProceed();
    }

    if (expect_report ==
        certificate_reporting_test_utils::CERT_REPORT_EXPECTED) {
      // Check that the mock reporter received a request to send a report.
      run_loop.Run();
      EXPECT_EQ(https_server_expired_.GetURL("/title1.html").host(),
                reporter_callback.GetLatestHostnameReported());
      EXPECT_NE(certificate_reporting::CertLoggerRequest::CHROME_CHANNEL_NONE,
                reporter_callback.GetLatestChromeChannelReported());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
      EXPECT_EQ(certificate_reporting::CertLoggerRequest::CHROME_CHANNEL_NONE,
                reporter_callback.GetLatestChromeChannelReported());
    }
  }

  // Helper function for testing invalid certificate chain reporting with the
  // bad clock interstitial.
  void TestBadClockReporting(
      certificate_reporting_test_utils::OptIn opt_in,
      certificate_reporting_test_utils::ExpectReport expect_report,
      Browser* browser) {
    ASSERT_TRUE(https_server_expired_.Start());

    base::RunLoop run_loop;
    certificate_reporting_test_utils::SSLCertReporterCallback reporter_callback(
        &run_loop);

    // Set network time back ten minutes, which is sufficient to
    // trigger the reporting.
    g_browser_process->network_time_tracker()->UpdateNetworkTime(
        base::Time::Now() - base::TimeDelta::FromMinutes(10),
        base::TimeDelta::FromMilliseconds(1),   /* resolution */
        base::TimeDelta::FromMilliseconds(500), /* latency */
        base::TimeTicks::Now() /* posting time of this update */);

    // Opt in to sending reports for invalid certificate chains.
    certificate_reporting_test_utils::SetCertReportingOptIn(browser, opt_in);

    ui_test_utils::NavigateToURL(browser,
                                 https_server_expired_.GetURL("/title1.html"));

    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                   AuthState::SHOWING_INTERSTITIAL);

    std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
        certificate_reporting_test_utils::CreateMockSSLCertReporter(
            base::Bind(&certificate_reporting_test_utils::
                           SSLCertReporterCallback::ReportSent,
                       base::Unretained(&reporter_callback)),
            expect_report);

    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(BadClockBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    BadClockBlockingPage* clock_page = static_cast<BadClockBlockingPage*>(
        tab->GetInterstitialPage()->GetDelegateForTesting());
    clock_page->SetSSLCertReporterForTesting(std::move(ssl_cert_reporter));

    EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
    interstitial_page->DontProceed();

    if (expect_report ==
        certificate_reporting_test_utils::CERT_REPORT_EXPECTED) {
      // Check that the mock reporter received a request to send a report.
      run_loop.Run();
      EXPECT_EQ(https_server_expired_.GetURL("/title1.html").host(),
                reporter_callback.GetLatestHostnameReported());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(std::string(), reporter_callback.GetLatestHostnameReported());
    }
  }

  void UpdateChromePolicy(const policy::PolicyMap& policies) {
    policy_provider_.UpdateChromePolicy(policies);
    DCHECK(base::MessageLoop::current());
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  void EnableRevocationChecking() {
    policy::PolicyMap policy_map;
    policy_map.Set(policy::key::kEnableOnlineRevocationChecks,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::Value>(true), nullptr);
    UpdateChromePolicy(policy_map);

    EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
        ssl_config::prefs::kCertRevocationCheckingEnabled));
    EXPECT_TRUE(g_browser_process->local_state()->IsManagedPreference(
        ssl_config::prefs::kCertRevocationCheckingEnabled));
  }

  // Helper function for TestInterstitialLinksOpenInNewTab. Implemented as a
  // test fixture method because the whole test fixture class is friended by
  // SSLBlockingPage.
  security_interstitials::SecurityInterstitialControllerClient*
  GetControllerClientFromInterstitialPage(SSLBlockingPage* ssl_interstitial) {
    return ssl_interstitial->controller();
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_server_expired_;
  net::EmbeddedTestServer https_server_mismatched_;
  net::SpawnedTestServer https_server_ocsp_ok_;
  net::SpawnedTestServer https_server_ocsp_revoked_;
  net::SpawnedTestServer wss_server_expired_;
  net::SpawnedTestServer wss_server_mismatched_;

 protected:
  // Navigates to an interstitial and clicks through the certificate
  // error; then navigates to a page at |path| that loads unsafe content.
  void SetUpUnsafeContentsWithUserException(const std::string& path) {
    ASSERT_TRUE(https_server_.Start());
    // Note that it is necessary to user https_server_mismatched_ here over the
    // other invalid cert servers. This is because the test relies on the two
    // servers having different hosts since SSL exceptions are per-host, not per
    // origin, and https_server_mismatched_ uses 'localhost' rather than
    // '127.0.0.1'.
    ASSERT_TRUE(https_server_mismatched_.Start());

    // Navigate to an unsafe site. Proceed with interstitial page to indicate
    // the user approves the bad certificate.
    ui_test_utils::NavigateToURL(
        browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                   AuthState::SHOWING_INTERSTITIAL);
    ProceedThroughInterstitial(tab);
    CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                   AuthState::NONE);

    std::string replacement_path;
    GetFilePathWithHostAndPortReplacement(
        path, https_server_mismatched_.host_port_pair(), &replacement_path);
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL(replacement_path));
  }

 private:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  policy::MockConfigurationPolicyProvider policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

class SSLUITestBlock : public SSLUITest {
 public:
  SSLUITestBlock() : SSLUITest() {}

  // Browser will not run insecure content.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // By overriding SSLUITest, we won't apply the flag that allows running
    // insecure content.
  }
};

class SSLUITestIgnoreCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrors() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will ignore certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

static std::string MakeCertSPKIFingerprint(net::X509Certificate* cert) {
  net::HashValue hash = GetSPKIHash(cert);
  std::string hash_base64;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(hash.data()),
                        hash.size()),
      &hash_base64);
  return hash_base64;
}

class SSLUITestIgnoreCertErrorsBySPKIHTTPS : public SSLUITest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::string whitelist_flag = MakeCertSPKIFingerprint(
        https_server_mismatched_.GetCertificate().get());
    // Browser will ignore certificate errors for chains matching one of the
    // public keys from the list.
    command_line->AppendSwitchASCII(switches::kIgnoreCertificateErrorsSPKIList,
                                    whitelist_flag);
  }
};

class SSLUITestIgnoreCertErrorsBySPKIWSS : public SSLUITest {
 public:
  SSLUITestIgnoreCertErrorsBySPKIWSS() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::string whitelist_flag =
        MakeCertSPKIFingerprint(wss_server_expired_.GetCertificate().get());
    // Browser will ignore certificate errors for chains matching one of the
    // public keys from the list.
    command_line->AppendSwitchASCII(switches::kIgnoreCertificateErrorsSPKIList,
                                    whitelist_flag);
  }
};

class SSLUITestIgnoreLocalhostCertErrors : public SSLUITest {
 public:
  SSLUITestIgnoreLocalhostCertErrors() : SSLUITest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will ignore certificate errors on localhost.
    command_line->AppendSwitch(switches::kAllowInsecureLocalhost);
  }
};

class SSLUITestWithExtendedReporting : public SSLUITest {
 public:
  SSLUITestWithExtendedReporting() : SSLUITest() {
    // Certificate reports are only sent from official builds, unless this has
    // been called.
    CertReportHelper::SetFakeOfficialBuildForTesting();
  }
};

class SSLUITestHSTS : public SSLUITest {
 public:
  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &SetHSTSForHostName,
            base::RetainedRef(browser()->profile()->GetRequestContext())));
  }
};

// Visits a regular page over http.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));

  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

// Visits a page over http which includes broken https resources (status should
// be OK).
// TODO(jcampan): test that bad HTTPS content is blocked (otherwise we'll give
//                the secure cookies away!).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPWithBrokenHTTPSResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement("/ssl/page_with_unsafe_contents.html",
                                        https_server_expired_.host_port_pair(),
                                        &replacement_path);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path));

  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_DATE_INVALID,
                                 AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Tests that the NavigationEntry gets marked as active mixed content,
// even if there is a certificate error. Regression test for
// https://crbug.com/593950.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSWithActiveInsecureContent) {
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error and click through the
  // interstitial.
  ui_test_utils::NavigateToURL(
      browser(),
      https_server_expired_.GetURL("/ssl/page_runs_insecure_content.html"));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);

  // Now check that the page is marked as having run insecure content.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 AuthState::RAN_INSECURE_CONTENT);
}

namespace {

// A WebContentsObserver that allows the user to wait for a
// DidChangeVisibleSecurityState event.
class SecurityStateWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit SecurityStateWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~SecurityStateWebContentsObserver() override {}

  void WaitForDidChangeVisibleSecurityState() { run_loop_.Run(); }

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};

// A WebContentsObserver that allows the user to wait for a same-document
// navigation. Tests using this observer will fail if a non-same-document
// navigation completes after calling WaitForSameDocumentNavigation.
class SameDocumentNavigationObserver : public content::WebContentsObserver {
 public:
  explicit SameDocumentNavigationObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~SameDocumentNavigationObserver() override {}

  void WaitForSameDocumentNavigation() { run_loop_.Run(); }

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_TRUE(navigation_handle->IsSameDocument());
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// Tests that the mixed content flags are reset when going back to an existing
// navigation entry that had mixed content. Regression test for
// https://crbug.com/750649.
IN_PROC_BROWSER_TEST_F(SSLUITest, GoBackToMixedContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  // Navigate to a URL and dynamically load mixed content.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckAuthenticatedState(tab, AuthState::NONE);
  SecurityStateWebContentsObserver observer(tab);
  ASSERT_TRUE(content::ExecuteScript(tab,
                                     "var i = document.createElement('img');"
                                     "i.src = 'http://example.test';"
                                     "document.body.appendChild(i);"));
  observer.WaitForDidChangeVisibleSecurityState();
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);

  // Now navigate somewhere else, and then back to the page that dynamically
  // loaded mixed content.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  // After going back, the mixed content indicator should no longer be present.
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Tests that the mixed content flags are not reset for an in-page navigation.
IN_PROC_BROWSER_TEST_F(SSLUITest, MixedContentWithSameDocumentNavigation) {
  ASSERT_TRUE(https_server_.Start());

  // Navigate to a URL and dynamically load mixed content.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  CheckAuthenticatedState(tab, AuthState::NONE);
  SecurityStateWebContentsObserver security_state_observer(tab);
  ASSERT_TRUE(content::ExecuteScript(tab,
                                     "var i = document.createElement('img');"
                                     "i.src = 'http://example.test';"
                                     "document.body.appendChild(i);"));
  security_state_observer.WaitForDidChangeVisibleSecurityState();
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);

  // Initiate a same-document navigation and check that the page is still
  // marked as having displayed mixed content.
  SameDocumentNavigationObserver navigation_observer(tab);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html#foo"));
  navigation_observer.WaitForSameDocumentNavigation();
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Tests that the WebContents's flag for displaying content with cert
// errors get cleared upon navigation.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       DisplayedContentWithCertErrorsClearedOnNavigation) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Navigate to a page with a certificate error and click through the
  // interstitial.
  ui_test_utils::NavigateToURL(
      browser(),
      https_server_expired_.GetURL("/ssl/page_with_subresource.html"));
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);

  NavigationEntry* entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetSSL().content_status &
              content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);

  // Navigate away to a different page, and check that the flag gets cleared.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  entry = tab->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_FALSE(entry->GetSSL().content_status &
               content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_Proceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  content::WaitForInterstitialAttach(
      browser()->tab_strip_model()->GetActiveWebContents());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          "1");
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::PROCEED, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_DontProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  content::WaitForInterstitialAttach(
      browser()->tab_strip_model()->GetActiveWebContents());
  histograms.ExpectTotalCount(decision_histogram, 1);
  histograms.ExpectBucketCount(decision_histogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);

  // Decision should be recorded.
  SendInterstitialCommand(browser()->tab_strip_model()->GetActiveWebContents(),
                          "0");
  histograms.ExpectTotalCount(decision_histogram, 2);
  histograms.ExpectBucketCount(
      decision_histogram, security_interstitials::MetricsHelper::DONT_PROCEED,
      1);
  histograms.ExpectTotalCount(interaction_histogram, 1);
  histograms.ExpectBucketCount(
      interaction_histogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestOKHTTPS) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Visits a page with https error and proceed:
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitialAttach(tab);
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialCrossSiteNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First navigate to an OK page.
  GURL initial_url = https_server_.GetURL("/ssl/google.html");
  ASSERT_EQ("127.0.0.1", initial_url.host());
  ui_test_utils::NavigateToURL(browser(), initial_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);

  // Navigate from 127.0.0.1 to localhost so it triggers a
  // cross-site navigation to make sure http://crbug.com/5800 is gone.
  GURL cross_site_url = https_server_mismatched_.GetURL("/ssl/google.html");
  ASSERT_EQ("localhost", cross_site_url.host());
  ui_test_utils::NavigateToURL(browser(), cross_site_url);
  // An interstitial should be showing.
  WaitForInterstitialAttach(tab);
  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking "Take me back".
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  interstitial_page->DontProceed();
  WaitForInterstitialDetach(tab);

  // We should be back to the original good page.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a new page to make sure bug 5800 is fixed.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Test that localhost pages don't show an interstitial.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreLocalhostCertErrors,
                       TestNoInterstitialOnLocalhost) {
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a localhost page.
  GURL url = https_server_.GetURL("/ssl/page_with_subresource.html");
  GURL::Replacements replacements;
  std::string new_host("localhost");
  replacements.SetHostStr(new_host);
  url = url.ReplaceComponents(replacements);

  ui_test_utils::NavigateToURL(browser(), url);

  // We should see no interstitial, but we should have an error
  // (red-crossed-out-https) in the URL bar.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::NONE);

  // We should see that the script tag in the page loaded and ran (and
  // wasn't blocked by the certificate error).
  base::string16 title;
  base::string16 expected_title = base::ASCIIToUTF16("This script has loaded");
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, expected_title);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorCausedByClockUsingBuildTime) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Set up the build and current clock times to be more than a year apart.
  std::unique_ptr<base::SimpleTestClock> mock_clock(
      new base::SimpleTestClock());
  mock_clock->SetNow(base::Time::NowFromSystemTime());
  mock_clock->Advance(base::TimeDelta::FromDays(367));
  SSLErrorHandler::SetClockForTesting(mock_clock.get());
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForInterstitialAttach(clock_tab);
  InterstitialPage* clock_interstitial = clock_tab->GetInterstitialPage();
  ASSERT_TRUE(clock_interstitial);
  EXPECT_EQ(BadClockBlockingPage::kTypeForTesting,
            clock_interstitial->GetDelegateForTesting()->GetTypeForTesting());
  CheckSecurityState(clock_tab, net::CERT_STATUS_DATE_INVALID,
                     security_state::DANGEROUS,
                     AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorCausedByClockUsingNetwork) {
  ASSERT_TRUE(https_server_expired_.Start());

  // Set network forward ten minutes, which is sufficient to trigger
  // the interstitial.
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      base::Time::Now() + base::TimeDelta::FromMinutes(10),
      base::TimeDelta::FromMilliseconds(1),   /* resolution */
      base::TimeDelta::FromMilliseconds(500), /* latency */
      base::TimeTicks::Now() /* posting time of this update */);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForInterstitialAttach(clock_tab);
  InterstitialPage* clock_interstitial = clock_tab->GetInterstitialPage();
  ASSERT_TRUE(clock_interstitial);
  EXPECT_EQ(BadClockBlockingPage::kTypeForTesting,
            clock_interstitial->GetDelegateForTesting()->GetTypeForTesting());
  CheckSecurityState(clock_tab, net::CERT_STATUS_DATE_INVALID,
                     security_state::DANGEROUS,
                     AuthState::SHOWING_INTERSTITIAL);
}

// Visits a page with https error and then goes back using Browser::GoBack.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestHTTPSExpiredCertAndGoBackViaButton) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  content::RenderFrameHost* rfh = tab->GetMainFrame();
  ASSERT_TRUE(entry);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking on back button (crbug.com/39248).
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);

  // Make sure we haven't changed the previous RFH.  Prevents regression of
  // http://crbug.com/82667.
  EXPECT_EQ(rfh, tab->GetMainFrame());

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using GoToOffset.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestHTTPSExpiredCertAndGoBackViaMenu) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an HTTP page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking and holding on back button (crbug.com/37215).
  tab->GetController().GoToOffset(-1);

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes forward using GoToOffset.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSExpiredCertAndGoForward) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to two HTTP pages.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry1 = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/blank_page.html"));
  NavigationEntry* entry2 = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry2);

  // Now go back so that a page is in the forward history.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }
  ASSERT_TRUE(tab->GetController().CanGoForward());
  NavigationEntry* entry3 = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry1 == entry3);

  // Now go to a bad HTTPS page that shows an interstitial.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking and holding on forward button.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoToOffset(1);
    observer.Wait();
  }

  // We should be showing the second good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
  EXPECT_FALSE(tab->GetController().CanGoForward());
  NavigationEntry* entry4 = tab->GetController().GetActiveEntry();
  EXPECT_TRUE(entry2 == entry4);
}

// Visits a page with revocation checking enabled and a valid OCSP response.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSOCSPOk) {
  EnableRevocationChecking();

  ASSERT_TRUE(https_server_ocsp_ok_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_ocsp_ok_.GetURL("/ssl/google.html"));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);

  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetActiveWebContents()
                                        ->GetController()
                                        .GetActiveEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetSSL().cert_status &
              net::CERT_STATUS_REV_CHECKING_ENABLED);
}

// Visits a page with revocation checking enabled and a revoked OCSP response.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSOCSPRevoked) {
  EnableRevocationChecking();

  ASSERT_TRUE(https_server_ocsp_revoked_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_ocsp_revoked_.GetURL("/ssl/google.html"));

  CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      net::CERT_STATUS_REVOKED, AuthState::SHOWING_INTERSTITIAL);
}

// Visit a HTTP page which request WSS connection to a server providing invalid
// certificate. Close the page while WSS connection waits for SSLManager's
// response from UI thread.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCertAndClose) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Create GURLs to test pages.
  std::string master_url_path = base::StringPrintf(
      "%s?%d",
      embedded_test_server()->GetURL("/ssl/wss_close.html").spec().c_str(),
      wss_server_expired_.host_port_pair().port());
  GURL master_url(master_url_path);
  std::string slave_url_path =
      base::StringPrintf("%s?%d", embedded_test_server()
                                      ->GetURL("/ssl/wss_close_slave.html")
                                      .spec()
                                      .c_str(),
                         wss_server_expired_.host_port_pair().port());
  GURL slave_url(slave_url_path);

  // Create tabs and visit pages which keep on creating wss connections.
  WebContents* tabs[16];
  for (int i = 0; i < 16; ++i) {
    tabs[i] = chrome::AddSelectedTabWithURL(browser(), slave_url,
                                            ui::PAGE_TRANSITION_LINK);
  }
  chrome::SelectNextTab(browser());

  // Visit a page which waits for one TLS handshake failure.
  // The title will be changed to 'PASS'.
  ui_test_utils::NavigateToURL(browser(), master_url);
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));

  // Close tabs which contains the test page.
  for (int i = 0; i < 16; ++i)
    chrome::CloseWebContents(browser(), tabs[i], false);
  chrome::CloseWebContents(browser(), tab, false);
}

// Visit a HTTPS page and proceeds despite an invalid certificate. The page
// requests WSS connection to the same origin host to check if WSS connection
// share certificates policy with HTTPS correcly.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestWSSInvalidCertAndGoForward) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(browser(),
                               wss_server_expired_.GetURL("connect_check.html")
                                   .ReplaceComponents(replacements));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  // Proceed anyway.
  ProceedThroughInterstitial(tab);

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}

// Ensure that non-standard origins are marked as neutral when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkFileAsNonSecure) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsDangerous);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("file:///"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
}

// Ensure that about-protocol origins are marked as neutral when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkAboutAsNonSecure) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsDangerous);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
}

// Data URLs should always be marked as non-secure.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkDataAsNonSecure) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/plain,hello"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
}

// Ensure that HTTP-protocol origins are marked as Dangerous when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkHTTPAsDangerous) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsDangerous);

  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a non-local HTTP page.
  GURL url = embedded_test_server()->GetURL("/ssl/google.html");
  GURL::Replacements http_url_replacements;
  http_url_replacements.SetHostStr("example.test");
  url = url.ReplaceComponents(http_url_replacements);
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);

  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
}

// Ensure that blob-protocol origins are marked as neutral when the
// MarkNonSecureAs Dangerous flag is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkBlobAsNonSecure) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      security_state::switches::kMarkHttpAs,
      security_state::switches::kMarkHttpAsDangerous);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  ASSERT_TRUE(helper);

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("blob:chrome://newtab/49a463bb-fac8-476c-97bf-5d7076c3ea1a"));
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
}

#if defined(USE_NSS_CERTS)
class SSLUITestWithClientCert : public SSLUITest {
 public:
  SSLUITestWithClientCert() : cert_db_(NULL) {}

  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();

    base::RunLoop loop;
    GetNSSCertDatabaseForProfile(
        browser()->profile(),
        base::Bind(&SSLUITestWithClientCert::DidGetCertDatabase,
                   base::Unretained(this),
                   &loop));
    loop.Run();
  }

 protected:
  void DidGetCertDatabase(base::RunLoop* loop, net::NSSCertDatabase* cert_db) {
    cert_db_ = cert_db;
    loop->Quit();
  }

  net::NSSCertDatabase* cert_db_;
};

// SSL client certificate tests are only enabled when using NSS for private key
// storage, as only NSS can avoid modifying global machine state when testing.
// See http://crbug.com/51132

// Visit a HTTPS page which requires client cert authentication. The client
// cert will be selected automatically, then a test which uses WebSocket runs.
IN_PROC_BROWSER_TEST_F(SSLUITestWithClientCert, TestWSSClientCert) {
  // Import a client cert for test.
  crypto::ScopedPK11Slot public_slot = cert_db_->GetPublicSlot();
  std::string pkcs12_data;
  base::FilePath cert_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_client_cert.p12"));
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    EXPECT_TRUE(base::ReadFileToString(cert_path, &pkcs12_data));
  }
  EXPECT_EQ(net::OK,
            cert_db_->ImportFromPKCS12(public_slot.get(), pkcs12_data,
                                       base::string16(), true, nullptr));

  // Start WebSocket test server with TLS and client cert authentication.
  net::SpawnedTestServer::SSLOptions options(
      net::SpawnedTestServer::SSLOptions::CERT_OK);
  options.request_client_certificate = true;
  base::FilePath ca_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_cacert.pem"));
  options.client_authorities.push_back(ca_path);
  net::SpawnedTestServer wss_server(net::SpawnedTestServer::TYPE_WSS,
                             options,
                             net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(wss_server.Start());
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  GURL url =
      wss_server.GetURL("connect_check.html").ReplaceComponents(replacements);

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Add an entry into AutoSelectCertificateForUrls policy for automatic client
  // cert selection.
  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("ISSUER.CN", "pywebsocket");
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetWebsiteSettingDefaultScope(
          url, GURL(), CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
          std::string(), std::move(dict));

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURL(browser(), url);
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Test page runs a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}
#endif  // defined(USE_NSS_CERTS)

// Open a page with a HTTPS error in a tab with no prior navigation (through a
// link with a blank target).  This is to test that the lack of navigation entry
// does not cause any problems (it was causing a crasher, see
// http://crbug.com/19941).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestHTTPSErrorWithNoNavEntry) {
  ASSERT_TRUE(https_server_expired_.Start());

  const GURL url = https_server_expired_.GetURL("/ssl/google.htm");
  WebContents* tab2 = chrome::AddSelectedTabWithURL(
      browser(), url, ui::PAGE_TRANSITION_TYPED);
  content::WaitForLoadStop(tab2);

  // Verify our assumption that there was no prior navigation.
  EXPECT_FALSE(chrome::CanGoBack(browser()));

  // We should have an interstitial page showing.
  WaitForInterstitialAttach(tab2);
  ASSERT_TRUE(tab2->GetInterstitialPage());
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting, tab2->GetInterstitialPage()
                                                  ->GetDelegateForTesting()
                                                  ->GetTypeForTesting());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadHTTPSDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  GURL url_non_dangerous = embedded_test_server()->GetURL("/title1.html");
  GURL url_dangerous =
      https_server_expired_.GetURL("/downloads/dangerous/dangerous.exe");
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir downloads_directory_;

  // Need empty temp dir to avoid having Chrome ask us for a new filename
  // when we've downloaded dangerous.exe one hundred times.
  ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, downloads_directory_.GetPath());

  // Visit a non-dangerous page.
  ui_test_utils::NavigateToURL(browser(), url_non_dangerous);

  // Now, start a transition to dangerous download.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::NavigateParams navigate_params(browser(), url_dangerous,
                                           ui::PAGE_TRANSITION_TYPED);
    chrome::Navigate(&navigate_params);
    observer.Wait();
  }

  // To exit the browser cleanly (and this test) we need to complete the
  // download after completing this test.
  content::DownloadTestObserverTerminal dangerous_download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);

  // Proceed through the SSL interstitial. This doesn't use
  // |ProceedThroughInterstitial| since no page load will commit.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab != NULL);
  ASSERT_TRUE(tab->GetInterstitialPage() != NULL);
  ASSERT_EQ(
      SSLBlockingPage::kTypeForTesting,
      tab->GetInterstitialPage()->GetDelegateForTesting()->GetTypeForTesting());
  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_DOWNLOAD_INITIATED,
        content::NotificationService::AllSources());
    tab->GetInterstitialPage()->Proceed();
    observer.Wait();
  }

  // There should still be an interstitial at this point. Press the
  // back button on the browser. Note that this doesn't wait for a
  // NAV_ENTRY_COMMITTED notification because going back with an
  // active interstitial simply hides the interstitial.
  ASSERT_TRUE(tab->GetInterstitialPage() != NULL);
  ASSERT_EQ(
      SSLBlockingPage::kTypeForTesting,
      tab->GetInterstitialPage()->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);

  dangerous_download_observer.WaitForFinished();
}

//
// Insecure content
//

// Visits a page that displays insecure content.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  // Load a page that displays insecure content.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckSecurityState(browser()->tab_strip_model()->GetActiveWebContents(),
                     CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page that displays an insecure form.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureForm) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_form.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckSecurityState(browser()->tab_strip_model()->GetActiveWebContents(),
                     CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

// Test that if the user proceeds and the checkbox is checked, a report
// is sent or not sent depending on the Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSProceedReporting) {
  certificate_reporting_test_utils::ExpectReport expect_report =
      certificate_reporting_test_utils::GetReportExpectedFromFinch();
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_PROCEED, expect_report, browser());
}

// Test that if the user goes back and the checkbox is checked, a report
// is sent or not sent depending on the Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSGoBackReporting) {
  certificate_reporting_test_utils::ExpectReport expect_report =
      certificate_reporting_test_utils::GetReportExpectedFromFinch();
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_DO_NOT_PROCEED, expect_report, browser());
}

// User proceeds, checkbox is shown but unchecked. Reports should never
// be sent, regardless of Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSProceedReportingWithNoOptIn) {
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN,
      SSL_INTERSTITIAL_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// User goes back, checkbox is shown but unchecked. Reports should never
// be sent, regardless of Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSGoBackShowYesCheckNoParamYesReportNo) {
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN,
      SSL_INTERSTITIAL_DO_NOT_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// User proceeds, checkbox is not shown but checked -> we expect no
// report.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSProceedShowNoCheckYesReportNo) {
  if (base::FieldTrialList::FindFullName(
          CertReportHelper::kFinchExperimentName) ==
      CertReportHelper::kFinchGroupDontShowDontSend) {
    TestBrokenHTTPSReporting(
        certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
        SSL_INTERSTITIAL_PROCEED,
        certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
  }
}

// Browser is incognito, user proceeds, checkbox has previously opted in
// -> no report, regardless of Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSInIncognitoReportNo) {
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED,
      CreateIncognitoBrowser());
}

// Test that reports don't get sent when extended reporting opt-in is
// disabled by policy.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBrokenHTTPSNoReportingWhenDisallowed) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      SSL_INTERSTITIAL_PROCEED,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// Checkbox is shown but unchecked. Reports should never be sent, regardless of
// Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBadClockReportingWithNoOptIn) {
  TestBadClockReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN,
      certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED, browser());
}

// Test that when the interstitial closes and the checkbox is checked, a report
// is sent or not sent depending on the Finch config.
IN_PROC_BROWSER_TEST_F(SSLUITestWithExtendedReporting,
                       TestBadClockReportingWithOptIn) {
  certificate_reporting_test_utils::ExpectReport expect_report =
      certificate_reporting_test_utils::GetReportExpectedFromFinch();
  TestBadClockReporting(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN,
      expect_report, browser());
}

// Visits a page that runs insecure content and tries to suppress the insecure
// content warnings by randomizing location.hash.
// Based on http://crbug.com/8706
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       TestRunsInsecuredContentRandomizeHash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/ssl/page_runs_insecure_content.html"));

  CheckAuthenticationBrokenState(
      browser()->tab_strip_model()->GetActiveWebContents(), CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT);
}

// Visits an SSL page twice, once with subresources served over good SSL and
// once over bad SSL.
// - For the good SSL case, the iframe and images should be properly displayed.
// - For the bad SSL case, the iframe contents shouldn't be displayed and images
//   and scripts should be filtered out entirely.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContents) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  // Enable popups without user gesture.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS,
                                 CONTENT_SETTING_ALLOW);
  {
    // First visit the page with its iframe and subresources served over good
    // SSL. This is a sanity check to make sure these resources aren't already
    // broken in the good case.
    std::string replacement_path;
    GetFilePathWithHostAndPortReplacement("/ssl/page_with_unsafe_contents.html",
                                          https_server_.host_port_pair(),
                                          &replacement_path);
    ui_test_utils::BrowserAddedObserver popup_observer;
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL(replacement_path));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    // The state is expected to be authenticated.
    CheckAuthenticatedState(tab, AuthState::NONE);
    // The iframe should be able to open a popup.
    popup_observer.WaitForSingleNewBrowser();
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // In order to check that the image was loaded, check its width.
    // The actual image (Google logo) is 276 pixels wide.
    int img_width = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        tab, "window.domAutomationController.send(ImageWidth());", &img_width));
    EXPECT_EQ(img_width, 276);
    // Check that variable |foo| is set.
    bool js_result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(IsFooSet());", &js_result));
    EXPECT_TRUE(js_result);
  }
  {
    // Now visit the page with its iframe and subresources served over bad
    // SSL. Iframes, images, and scripts should all be blocked.
    std::string replacement_path;
    GetFilePathWithHostAndPortReplacement(
        "/ssl/page_with_unsafe_contents.html",
        https_server_expired_.host_port_pair(), &replacement_path);
    ui_test_utils::NavigateToURL(browser(),
                                 https_server_.GetURL(replacement_path));
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    // When the bad content is filtered, the state is expected to be
    // authenticated.
    CheckAuthenticatedState(tab, AuthState::NONE);
    // The iframe attempts to open a popup window, but it shouldn't be able to.
    // Previous popup is still open.
    EXPECT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
    // The broken image width is zero.
    int img_width = 99;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        tab, "window.domAutomationController.send(ImageWidth());", &img_width));
    EXPECT_EQ(img_width, 16);
    // Check that variable |foo| is not set.
    bool js_result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab, "window.domAutomationController.send(IsFooSet());", &js_result));
    EXPECT_FALSE(js_result);
  }
}

// Visits a page with insecure content loaded by JS (after the initial page
// load).
#if defined(OS_LINUX)
// flaky http://crbug.com/396462
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
    DISABLED_TestDisplaysInsecureContentLoadedFromJS
#else
#define MAYBE_TestDisplaysInsecureContentLoadedFromJS \
    TestDisplaysInsecureContentLoadedFromJS
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       MAYBE_TestDisplaysInsecureContentLoadedFromJS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  net::HostPortPair replacement_pair = embedded_test_server()->host_port_pair();
  replacement_pair.set_host("example.test");

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html",
      replacement_pair, &replacement_path);
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Load the insecure image.
  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "loadBadImage();",
      &js_result));
  EXPECT_TRUE(js_result);

  // We should now have insecure content.
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits two pages from the same origin: one that displays insecure content and
// one that doesn't.  The test checks that we do not propagate the insecure
// content state from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysInsecureContentTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/blank_page.html"));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  CheckAuthenticatedState(tab1, AuthState::NONE);

  // Create a new tab.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  GURL url = https_server_.GetURL(replacement_path);
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Navigate(&params);
  WebContents* tab2 = params.target_contents;
  observer.Wait();

  // The new tab has insecure content.
  CheckSecurityState(tab2, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);

  // The original tab should not be contaminated.
  CheckAuthenticatedState(tab1, AuthState::NONE);
}

// Visits two pages from the same origin: one that runs insecure content and one
// that doesn't.  The test checks that we propagate the insecure content state
// from one to the other.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsInsecureContentTwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/blank_page.html"));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // This tab should be fine.
  CheckAuthenticatedState(tab1, AuthState::NONE);

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  // Create a new tab in the same process.  Using a NEW_FOREGROUND_TAB
  // disposition won't usually stay in the same process, but this works
  // because we are using process-per-site in SetUpCommandLine.
  GURL url = https_server_.GetURL(replacement_path);
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Navigate(&params);
  WebContents* tab2 = params.target_contents;
  observer.Wait();

  // Both tabs should have the same process.
  EXPECT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess());

  // The new tab has insecure content.
  CheckAuthenticationBrokenState(tab2, CertError::NONE,
                                 AuthState::RAN_INSECURE_CONTENT);

  // Which means the origin for the first tab has also been contaminated with
  // insecure content.
  CheckAuthenticationBrokenState(
      tab1, CertError::NONE, AuthState::RAN_INSECURE_CONTENT);
}

// Visits a page with an image over http.  Visits another page over https
// referencing that same image over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestDisplaysCachedInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  // Load original page over HTTP.
  const GURL url_http = embedded_test_server()->GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_http);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page with script over http.  Visits another page over https
// referencing that same script over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRunsCachedInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  // Load original page over HTTP.
  const GURL url_http = embedded_test_server()->GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_http);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Load again but over SSL.  It should be marked as displaying insecure
  // content (even though the image comes from the WebCore memory cache).
  const GURL url_https = https_server_.GetURL(replacement_path);
  ui_test_utils::NavigateToURL(browser(), url_https);
  CheckAuthenticationBrokenState(tab, CertError::NONE,
                                 AuthState::RAN_INSECURE_CONTENT);
}

// This test ensures the CN invalid status does not 'stick' to a certificate
// (see bug #1044942) and that it depends on the host-name.
// Test if disabled due to flakiness http://crbug.com/368280 .
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCNInvalidStickiness) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  // First we hit the server with hostname, this generates an invalid policy
  // error.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/google.html"));

  // We get an interstitial page as a result.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);

  // Now we try again with the right host name this time.
  GURL url(https_server_.GetURL("/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser(), url);

  // Security state should be OK.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Now try again the broken one to make sure it is still broken.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/google.html"));

  // Since we OKed the interstitial last time, we get right to the page.
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_COMMON_NAME_INVALID, AuthState::NONE);
}

// Test that navigating to a #ref does not change a bad security state.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRefNavigation) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/page_with_refs.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
  // Now navigate to a ref in the page, the security state should not have
  // changed.
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/page_with_refs.html#jp"));

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Tests that closing a page that opened a pop-up with an interstitial does not
// crash the browser (crbug.com/1966).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestCloseTabWithUnsafePopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // Enable popups without user gesture.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS,
                                 CONTENT_SETTING_ALLOW);

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement("/ssl/page_with_unsafe_popup.html",
                                        https_server_expired_.host_port_pair(),
                                        &replacement_path);
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  content::WindowedNotificationObserver popup_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::WindowedNotificationObserver nav_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path));
  // Wait for popup window to appear and finish navigating.
  popup_observer.Wait();
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Last activated browser should be the popup.
  Browser* popup_browser = chrome::FindBrowserWithProfile(browser()->profile());
  WebContents* popup = popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(popup, tab1);
  nav_observer.Wait();
  // Since the popup is showing an interstitial, it shouldn't have a last
  // committed entry.
  content::WaitForInterstitialAttach(popup);
  EXPECT_FALSE(popup->GetController().GetLastCommittedEntry());
  ASSERT_TRUE(popup->GetController().GetVisibleEntry());
  EXPECT_EQ(https_server_expired_.GetURL("/ssl/bad_iframe.html"),
            popup->GetController().GetVisibleEntry()->GetURL());
  EXPECT_TRUE(popup->ShowingInterstitialPage());

  // Add another tab to make sure the browser does not exit when we close
  // the first tab.
  GURL url = embedded_test_server()->GetURL("/ssl/google.html");
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Close the first tab.
  chrome::CloseWebContents(browser(), tab1, false);
}

// Visit a page over bad https that is a redirect to a page with good https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectBadToGoodHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_expired_.GetURL("/server-redirect?");
  GURL url2 = https_server_.GetURL("/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // We have been redirected to the good page.
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Visit a page over good https that is a redirect to a page with bad https.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectGoodToBadHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_.GetURL("/server-redirect?");
  GURL url2 = https_server_expired_.GetURL("/ssl/google.html");
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));
  content::WaitForInterstitialAttach(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with good HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToGoodHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // HTTP redirects to good HTTPS.
  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL good_https_url = https_server_.GetURL("/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + good_https_url.spec()));
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPToBadHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  const GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  const GURL bad_https_url = https_server_expired_.GetURL("/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + bad_https_url.spec()));
  WaitForInterstitialAttach(tab);
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Visit a page over https that is a redirect to a page with http (to make sure
// we don't keep the secure state).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestRedirectHTTPSToHTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/server-redirect?");
  GURL http_url = embedded_test_server()->GetURL("/ssl/google.html");

  ui_test_utils::NavigateToURL(browser(),
                               GURL(https_url.spec() + http_url.spec()));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(), AuthState::NONE);
}

class SSLUITestWaitForDOMNotification : public SSLUITestIgnoreCertErrors,
                                        public content::NotificationObserver {
 public:
  SSLUITestWaitForDOMNotification()
      : SSLUITestIgnoreCertErrors(), run_loop_(nullptr) {}

  ~SSLUITestWaitForDOMNotification() override { registrar_.RemoveAll(); };

  void SetUpOnMainThread() override {
    SSLUITestIgnoreCertErrors::SetUpOnMainThread();
    registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::NotificationService::AllSources());
  }

  void set_expected_notification(const std::string& expected_notification) {
    expected_notification_ = expected_notification;
  }

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

  // content::NotificationObserver
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK(run_loop_);
    if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
      content::Details<std::string> dom_op_result(details);
      if (*dom_op_result.ptr() == expected_notification_) {
        run_loop_->QuitClosure().Run();
      }
    }
  }

 private:
  content::NotificationRegistrar registrar_;
  std::string expected_notification_;
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SSLUITestWaitForDOMNotification);
};

// Tests that a mixed resource which includes HTTP in the redirect chain
// is marked as mixed content, even if the end result is HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITestWaitForDOMNotification,
                       TestMixedContentWithHTTPInRedirectChain) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/blank_page.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Construct a URL which will be dynamically added to the page as an
  // image. The URL redirects through HTTP, though it ends up at an
  // HTTPS resource.
  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL::Replacements http_url_replacements;
  // Be sure to use a non-localhost name for the mixed content request,
  // since local hostnames are not considered mixed content.
  http_url_replacements.SetHostStr("example.test");
  std::string http_url_query =
      EncodeQuery(https_server_.GetURL("/ssl/google_files/logo.gif").spec());
  http_url_replacements.SetQueryStr(http_url_query);
  http_url = http_url.ReplaceComponents(http_url_replacements);

  GURL https_url = https_server_.GetURL("/server-redirect?");
  GURL::Replacements https_url_replacements;
  std::string https_url_query = EncodeQuery(http_url.spec());
  https_url_replacements.SetQueryStr(https_url_query);
  https_url = https_url.ReplaceComponents(https_url_replacements);

  base::RunLoop run_loop;

  // Load the image. It starts at |https_server_|, which redirects to an
  // embedded_test_server() HTTP URL, which redirects back to
  // |https_server_| for the final HTTPS image. Because the redirect
  // chain passes through HTTP, the page should be marked as mixed
  // content.
  set_expected_notification("\"mixed-image-loaded\"");
  set_run_loop(&run_loop);
  ASSERT_TRUE(content::ExecuteScript(
      tab,
      "var loaded = function () {"
      "  window.domAutomationController.send('mixed-image-loaded');"
      "};"
      "var img = document.createElement('img');"
      "img.onload = loaded;"
      "img.src = '" +
          https_url.spec() + "';"
                             "document.body.appendChild(img);"));

  run_loop.Run();
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Visits a page to which we could not connect (bad port) over http and https
// and make sure the security style is correct.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestConnectToBadPort) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://localhost:17"));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);

  // Same thing over HTTPS.
  ui_test_utils::NavigateToURL(browser(), GURL("https://localhost:17"));
  CheckUnauthenticatedState(
      browser()->tab_strip_model()->GetActiveWebContents(),
      AuthState::SHOWING_ERROR);
}

//
// Frame navigation
//

// From a good HTTPS top frame:
// - navigate to an OK HTTPS frame
// - navigate to a bad HTTPS (expect unsafe content and filtered frame), then
//   back
// - navigate to HTTP (expect insecure content), then back
IN_PROC_BROWSER_TEST_F(SSLUITest, TestGoodFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // SetUpOnMainThread adds this hostname to the resolver so that it's not
  // blocked (browser_test_base.cc has a resolver that blocks all non-local
  // hostnames by default to ensure tests don't hit the network). This is
  // critical to do because for PlzNavigate the request would otherwise get
  // cancelled in the browser before the renderer sees it.

  std::string top_frame_path;
  GetTopFramePath(*embedded_test_server(), https_server_, https_server_expired_,
                  &top_frame_path);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(top_frame_path));

  CheckAuthenticatedState(tab, AuthState::NONE);

  bool success = false;
  // Now navigate inside the frame.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // We should still be fine.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Now let's hit a bad page.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('badHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // The security style should still be secure.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // And the frame should be blocked.
  bool is_content_evil = true;
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
        tab, base::Bind(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js("window.domAutomationController.send("
                         "document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(content_frame,
                                                   is_evil_js,
                                                   &is_content_evil));
  EXPECT_FALSE(is_content_evil);

  // Now go back, our state should still be OK.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Navigate to a page served over HTTP.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('HTTPLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // Our state should be unauthenticated (in the ran mixed script sense). Note
  // this also displays images from the http page (google.com).
  CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT |
          AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  // Go back, our state should be unchanged.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }

  CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT |
          AuthState::DISPLAYED_FORM_WITH_INSECURE_ACTION);
}

// From a bad HTTPS top frame:
// - navigate to an OK HTTPS frame (expected to be still authentication broken).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  GetTopFramePath(*embedded_test_server(), https_server_, https_server_expired_,
                  &top_frame_path);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL(top_frame_path));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  // Navigate to a good frame.
  bool success = false;
  content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
      &success));
  ASSERT_TRUE(success);
  observer.Wait();

  // We should still be authentication broken.
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 AuthState::NONE);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnauthenticatedFrameNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string top_frame_path;
  GetTopFramePath(*embedded_test_server(), https_server_, https_server_expired_,
                  &top_frame_path);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(top_frame_path));
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate inside the frame to a secure HTTPS frame.
  {
    bool success = false;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('goodHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // We should still be unauthenticated.
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Now navigate to a bad HTTPS frame.
  {
    bool success = false;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(clickLink('badHTTPSLink'));",
        &success));
    ASSERT_TRUE(success);
    observer.Wait();
  }

  // State should not have changed.
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // And the frame should have been blocked (see bug #2316).
  bool is_content_evil = true;
  content::RenderFrameHost* content_frame = content::FrameMatchingPredicate(
        tab, base::Bind(&content::FrameMatchesName, "contentFrame"));
  std::string is_evil_js("window.domAutomationController.send("
                         "document.getElementById('evilDiv') != null);");
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(content_frame,
                                                   is_evil_js,
                                                   &is_content_evil));
  EXPECT_FALSE(is_content_evil);
}

enum class OffMainThreadFetchMode { kEnabled, kDisabled };
enum class SSLUIWorkerFetchTestType { kUseFetch, kUseImportScripts };

class SSLUIWorkerFetchTest
    : public testing::WithParamInterface<
          std::pair<OffMainThreadFetchMode, SSLUIWorkerFetchTestType>>,
      public SSLUITest {
 public:
  SSLUIWorkerFetchTest() { EXPECT_TRUE(tmp_dir_.CreateUniqueTempDir()); }
  ~SSLUIWorkerFetchTest() override {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().first == OffMainThreadFetchMode::kEnabled) {
      command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                      features::kOffMainThreadFetch.name);
    } else {
      command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                      features::kOffMainThreadFetch.name);
    }
  }

 protected:
  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    EXPECT_EQ(base::checked_cast<int>(contents.size()),
              base::WriteFile(tmp_dir_.GetPath().Append(filename),
                              contents.data(), contents.size()));
  }

  void WriteTestFiles(const net::EmbeddedTestServer& remote_server,
                      const std::string& hostname) {
    WriteFile(FILE_PATH_LITERAL("worker_test.html"),
              "<script>"
              "var worker = new Worker('worker.js');"
              "worker.addEventListener("
              "    'message',"
              "    event => { document.title = event.data; });"
              "</script>");
    switch (GetParam().second) {
      case SSLUIWorkerFetchTestType::kUseFetch:
        WriteFile(FILE_PATH_LITERAL("worker_test_data.txt.mock-http-headers"),
                  "HTTP/1.1 200 OK\n"
                  "Content-Type: text/plain\n"
                  "Access-Control-Allow-Origin: *");
        WriteFile(FILE_PATH_LITERAL("worker_test_data.txt"), "LOADED");
        WriteFile(FILE_PATH_LITERAL("worker.js"),
                  base::StringPrintf(
                      "fetch('%s')"
                      "  .then(res => res.text())"
                      "  .then(text => postMessage(text))"
                      "  .catch(_ => postMessage('FAILED'))",
                      remote_server.GetURL(hostname, "/worker_test_data.txt")
                          .spec()
                          .c_str()));
        break;
      case SSLUIWorkerFetchTestType::kUseImportScripts: {
        WriteFile(FILE_PATH_LITERAL("imported.js"), "data = 'LOADED';");
        WriteFile(
            FILE_PATH_LITERAL("worker.js"),
            base::StringPrintf(
                "var data = 'FAILED';"
                "try {"
                "  importScripts('%s')"
                "} catch(e) {}"
                "postMessage(data);",
                remote_server.GetURL(hostname, "/imported.js").spec().c_str()));
      } break;
    }
  }

  void RunMixedContentSettingsTest(
      ChromeContentBrowserClientForMixedContentTest* browser_client,
      bool allow_running_insecure_content,
      bool strict_mixed_content_checking,
      bool strictly_block_blockable_mixed_content,
      bool expected_load,
      bool expected_show_blocked,
      bool expected_show_dangerous,
      bool expected_load_after_allow,
      bool expected_show_blocked_after_allow,
      bool expected_show_dangerous_after_allow) {
    SCOPED_TRACE(
        ::testing::Message()
        << "RunMixedContentSettingsTest :"
        << "allow_running_insecure_content="
        << (allow_running_insecure_content ? "true " : "false ")
        << "strict_mixed_content_checking="
        << (strict_mixed_content_checking ? "true " : "false ")
        << "strictly_block_blockable_mixed_content="
        << (strictly_block_blockable_mixed_content ? "true " : "false "));

    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

    browser_client->SetMixedContentSettings(
        allow_running_insecure_content, strict_mixed_content_checking,
        strictly_block_blockable_mixed_content);

    // Clears the error state which may be set by the previous test case.
    ClearErrorState();

    const base::string16 loaded_title = base::ASCIIToUTF16("LOADED");
    const base::string16 failed_title = base::ASCIIToUTF16("FAILED");

    {
      // First load.
      content::TitleWatcher watcher(tab, loaded_title);
      watcher.AlsoWaitForTitle(failed_title);
      ui_test_utils::NavigateToURL(browser(),
                                   https_server_.GetURL("/worker_test.html"));
      EXPECT_EQ(expected_load ? loaded_title : failed_title,
                watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(
        expected_show_blocked,
        TabSpecificContentSettings::FromWebContents(tab)->IsContentBlocked(
            CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
    CheckSecurityState(tab, CertError::NONE,
                       expected_show_dangerous ? security_state::DANGEROUS
                                               : security_state::SECURE,
                       expected_show_dangerous ? AuthState::RAN_INSECURE_CONTENT
                                               : AuthState::NONE);
    // Clears title.
    ASSERT_TRUE(
        content::ExecuteScript(tab->GetMainFrame(), "document.title = \"\";"));

    {
      // SetAllowRunningInsecureContent will reload the page.
      content::TitleWatcher watcher(tab, loaded_title);
      watcher.AlsoWaitForTitle(failed_title);
      SetAllowRunningInsecureContent();
      EXPECT_EQ(expected_load_after_allow ? loaded_title : failed_title,
                watcher.WaitAndGetTitle());
    }

    EXPECT_EQ(
        expected_show_blocked_after_allow,
        TabSpecificContentSettings::FromWebContents(tab)->IsContentBlocked(
            CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
    CheckSecurityState(
        tab, CertError::NONE,
        expected_show_dangerous_after_allow ? security_state::DANGEROUS
                                            : security_state::SECURE,
        expected_show_dangerous_after_allow ? AuthState::RAN_INSECURE_CONTENT
                                            : AuthState::NONE);
  }

  base::ScopedTempDir tmp_dir_;

 private:
  void SetAllowRunningInsecureContent() {
    content::RenderFrameHost* render_frame_host =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    chrome::mojom::InsecureContentRendererPtr renderer;
    render_frame_host->GetRemoteInterfaces()->GetInterface(&renderer);
    renderer->SetAllowRunningInsecureContent();
  }

  void ClearErrorState() {
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(tab, 1);
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL("/empty.html"));
    observer.Wait();
    EXPECT_FALSE(
        TabSpecificContentSettings::FromWebContents(tab)->IsContentBlocked(
            CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
    CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                       AuthState::NONE);
  }
};

IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       TestUnsafeContentsInWorkerFiltered) {
  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  https_server_expired_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());
  WriteTestFiles(https_server_expired_, "localhost");

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  const base::string16 loaded_title = base::ASCIIToUTF16("LOADED");
  const base::string16 failed_title = base::ASCIIToUTF16("FAILED");
  content::TitleWatcher watcher(tab, loaded_title);
  watcher.AlsoWaitForTitle(failed_title);

  // This page will spawn a Worker which will try to load content from
  // BadCertServer.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/worker_test.html"));
  // Expect Worker not to load insecure content.
  EXPECT_EQ(failed_title, watcher.WaitAndGetTitle());
  // The bad content is filtered, expect the state to be authenticated.
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// This test, and the related test TestUnsafeContentsWithUserException, verify
// that if unsafe content is loaded but the host of that unsafe content has a
// user exception, the content runs and the security style is downgraded.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       TestUnsafeContentsInWorkerWithUserException) {
  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  https_server_mismatched_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  // Note that it is necessary to user https_server_mismatched_ here over the
  // other invalid cert servers. This is because the test relies on the two
  // servers having different hosts since SSL exceptions are per-host, not per
  // origin, and https_server_mismatched_ uses 'localhost' rather than
  // '127.0.0.1'.
  ASSERT_TRUE(https_server_mismatched_.Start());

  WriteTestFiles(https_server_mismatched_, "localhost");

  // Navigate to an unsafe site. Proceed with interstitial page to indicate
  // the user approves the bad certificate.
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);
  ProceedThroughInterstitial(tab);
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::NONE);

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.content_with_cert_errors_status);

  const base::string16 loaded_title = base::ASCIIToUTF16("LOADED");
  const base::string16 failed_title = base::ASCIIToUTF16("FAILED");
  content::TitleWatcher watcher(tab, loaded_title);
  watcher.AlsoWaitForTitle(failed_title);

  // Navigate to safe page that has Worker loading unsafe content.
  // Expect content to load but be marked as auth broken due to running insecure
  // content.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/worker_test.html"));
  // Worker loads insecure content
  EXPECT_EQ(loaded_title, watcher.WaitAndGetTitle());
  CheckAuthenticationBrokenState(tab, CertError::NONE, AuthState::NONE);

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_EQ(security_state::CONTENT_STATUS_RAN,
            security_info.content_with_cert_errors_status);
}

// Flaky on Windows 7 (dbg) trybot, see https://crbug.com/443374.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_MixedContentSettings DISABLED_MixedContentSettings
#else
#define MAYBE_MixedContentSettings MixedContentSettings
#endif

// This test checks the behavior of mixed content blocking for the requests
// from a dedicated worker by changing the settings in WebPreferences.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest, MAYBE_MixedContentSettings) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");

  for (bool allow_running_insecure_content : {true, false}) {
    for (bool strict_mixed_content_checking : {true, false}) {
      for (bool strictly_block_blockable_mixed_content : {true, false}) {
        if (strict_mixed_content_checking) {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, false /* expected_load */,
              false /* expected_show_blocked */,
              false /* expected_show_dangerous */,
              false /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              false /* expected_show_dangerous_after_allow */);
        } else if (allow_running_insecure_content) {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, true /* expected_load */,
              false /* expected_show_blocked */,
              true /* expected_show_dangerous */,
              true /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              true /* expected_show_dangerous_after_allow */);
        } else if (strictly_block_blockable_mixed_content) {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, false /* expected_load */,
              false /* expected_show_blocked */,
              false /* expected_show_dangerous */,
              false /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              false /* expected_show_dangerous_after_allow */);
        } else {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, false /* expected_load */,
              true /* expected_show_blocked */,
              false /* expected_show_dangerous */,
              true /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              true /* expected_show_dangerous_after_allow */);
        }
      }
    }
  }

  content::SetBrowserClientForTesting(old_browser_client);
}

// Flaky on Windows 7 (dbg) trybot, see https://crbug.com/443374.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_MixedContentSettingsWithBlockingCSP \
  DISABLED_MixedContentSettingsWithBlockingCSP
#else
#define MAYBE_MixedContentSettingsWithBlockingCSP \
  MixedContentSettingsWithBlockingCSP
#endif

// This test checks that all mixed content requests from a dedicated worker are
// blocked regardless of the settings in WebPreferences when
// block-all-mixed-content CSP is set.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest,
                       MAYBE_MixedContentSettingsWithBlockingCSP) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker_test.html.mock-http-headers"),
            "HTTP/1.1 200 OK\n"
            "Content-Type: text/html\n"
            "Content-Security-Policy: block-all-mixed-content;");

  for (bool allow_running_insecure_content : {true, false}) {
    for (bool strict_mixed_content_checking : {true, false}) {
      for (bool strictly_block_blockable_mixed_content : {true, false}) {
        RunMixedContentSettingsTest(
            &browser_client, allow_running_insecure_content,
            strict_mixed_content_checking,
            strictly_block_blockable_mixed_content, false /* expected_load */,
            false /* expected_show_blocked */,
            false /* expected_show_dangerous */,
            false /* expected_load_after_allow */,
            false /* expected_show_blocked_after_allow */,
            false /* expected_show_dangerous_after_allow */);
      }
    }
  }
  content::SetBrowserClientForTesting(old_browser_client);
}

// Flaky on Windows 7 (dbg) trybot, see https://crbug.com/443374.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_MixedContentSubFrame DISABLED_MixedContentSubFrame
#else
#define MAYBE_MixedContentSubFrame MixedContentSubFrame
#endif

// This test checks that all mixed content requests from a dedicated worker
// which is started from a subframe are blocked if
// allow_running_insecure_content setting is false or
// strict_mixed_content_checking setting is true.
IN_PROC_BROWSER_TEST_P(SSLUIWorkerFetchTest, MAYBE_MixedContentSubFrame) {
  ChromeContentBrowserClientForMixedContentTest browser_client;
  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  https_server_.ServeFilesFromDirectory(tmp_dir_.GetPath());
  embedded_test_server()->ServeFilesFromDirectory(tmp_dir_.GetPath());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  WriteTestFiles(*embedded_test_server(), "example.com");
  WriteFile(FILE_PATH_LITERAL("worker_iframe.html"),
            "<script>"
            "var worker = new Worker('worker.js');"
            "worker.addEventListener("
            "    'message',"
            "    event => { parent.postMessage(event.data, '*'); });"
            "</script>");
  WriteFile(FILE_PATH_LITERAL("worker_test.html"),
            "<script>"
            "window.addEventListener("
            "    'message',"
            "    event => { document.title = event.data; });"
            "</script>"
            "<iframe src=\"./worker_iframe.html\" />");

  for (bool allow_running_insecure_content : {true, false}) {
    for (bool strict_mixed_content_checking : {true, false}) {
      for (bool strictly_block_blockable_mixed_content : {true, false}) {
        if (allow_running_insecure_content && !strict_mixed_content_checking) {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, true /* expected_load */,
              false /* expected_show_blocked */,
              true /* expected_show_dangerous */,
              true /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              true /* expected_show_dangerous_after_allow */);
        } else {
          RunMixedContentSettingsTest(
              &browser_client, allow_running_insecure_content,
              strict_mixed_content_checking,
              strictly_block_blockable_mixed_content, false /* expected_load */,
              false /* expected_show_blocked */,
              false /* expected_show_dangerous */,
              false /* expected_load_after_allow */,
              false /* expected_show_blocked_after_allow */,
              false /* expected_show_dangerous_after_allow */);
        }
      }
    }
  }

  content::SetBrowserClientForTesting(old_browser_client);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    SSLUIWorkerFetchTest,
    ::testing::Values(
        std::make_pair(OffMainThreadFetchMode::kDisabled,
                       SSLUIWorkerFetchTestType::kUseFetch),
        std::make_pair(OffMainThreadFetchMode::kDisabled,
                       SSLUIWorkerFetchTestType::kUseImportScripts),
        std::make_pair(OffMainThreadFetchMode::kEnabled,
                       SSLUIWorkerFetchTestType::kUseFetch),
        std::make_pair(OffMainThreadFetchMode::kEnabled,
                       SSLUIWorkerFetchTestType::kUseImportScripts)));

// Visits a page with unsafe content and makes sure that if a user exception
// to the certificate error is present, the image is loaded and script
// executes.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(SetUpUnsafeContentsWithUserException(
      "/ssl/page_with_unsafe_contents.html"));
  CheckAuthenticationBrokenState(tab, CertError::NONE, AuthState::NONE);

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
            security_info.content_with_cert_errors_status);

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(img_width, 100);

  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "window.domAutomationController.send(IsFooSet());", &js_result));
  EXPECT_TRUE(js_result);

  // Test that active subresources with the same certificate errors as
  // the main resources also get noted in |content_with_cert_errors_status|.
  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_contents.html",
      https_server_mismatched_.host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL(replacement_path));
  js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab, "window.domAutomationController.send(IsFooSet());", &js_result));
  EXPECT_TRUE(js_result);
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::NONE);

  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED_AND_RAN,
            security_info.content_with_cert_errors_status);
}

// Like the test above, but only displaying inactive content (an image).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeImageWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(
      SetUpUnsafeContentsWithUserException("/ssl/page_with_unsafe_image.html"));

  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::CONTENT_STATUS_NONE,
            security_info.mixed_content_status);
  EXPECT_EQ(security_state::CONTENT_STATUS_DISPLAYED,
            security_info.content_with_cert_errors_status);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(0u, security_info.cert_status);

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(img_width, 100);
}

// Test that when the browser blocks displaying insecure content (iframes),
// the indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means)
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockDisplayingInsecureIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_iframe.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Test that when the browser blocks running insecure content, the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other
// means).
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockRunningInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_runs_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors. The connection should be established without
// interstitial page showing.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrors, TestWSS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(browser(),
                               wss_server_expired_.GetURL("connect_check.html")
                                   .ReplaceComponents(replacements));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}

// Visit a page and establish a WebSocket connection over bad https with
// --ignore-certificate-errors-spki-list. The connection should be established
// without interstitial page showing.
#if !defined(OS_CHROMEOS)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIWSS, TestWSSExpired) {
  ASSERT_TRUE(wss_server_expired_.Start());

  // Setup page title observer.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher watcher(tab, ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));

  // Visit bad HTTPS page.
  GURL::Replacements replacements;
  replacements.SetSchemeStr("https");
  ui_test_utils::NavigateToURL(browser(),
                               wss_server_expired_.GetURL("connect_check.html")
                                   .ReplaceComponents(replacements));

  // We shouldn't have an interstitial page showing here.

  // Test page run a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}
#endif  // !defined(OS_CHROMEOS)

// Test that HTTPS pages with a bad certificate don't show an interstitial if
// the public key matches a value from --ignore-certificate-errors-spki-list.
#if !defined(OS_CHROMEOS)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIHTTPS, TestHTTPS) {
  ASSERT_TRUE(https_server_mismatched_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURL(
      browser(),
      https_server_mismatched_.GetURL("/ssl/page_with_subresource.html"));

  // We should see no interstitial. The script tag in the page should have
  // loaded and ran (and wasn't blocked by the certificate error).
  CheckAuthenticatedState(tab, AuthState::NONE);
  base::string16 title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(title, base::ASCIIToUTF16("This script has loaded"));
}
#endif  // !defined(OS_CHROMEOS)

// Test subresources from an origin with a bad certificate are loaded if the
// public key matches a value from --ignore-certificate-errors-spki-list.
#if !defined(OS_CHROMEOS)  // Chrome OS does not support the flag.
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreCertErrorsBySPKIHTTPS,
                       TestInsecureSubresource) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_mismatched_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_unsafe_image.html",
      https_server_mismatched_.host_port_pair(), &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  // We should see no interstitial.
  CheckAuthenticatedState(tab, AuthState::NONE);
  // In order to check that the image was loaded, check its width.
  // The actual image (Google logo) is 276 pixels wide.
  int img_width = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  EXPECT_GT(img_width, 200);
}
#endif  // !defined(OS_CHROMEOS)

// Verifies that the interstitial can proceed, even if JavaScript is disabled.
// http://crbug.com/322948
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptProceeds) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(WaitForRenderFrameReady(interstitial_page->GetMainFrame()));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab->GetController()));
  int result = -1;
  const std::string javascript =
      base::StringPrintf("window.domAutomationController.send(%d);",
                         security_interstitials::CMD_PROCEED);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      interstitial_page->GetMainFrame(), javascript, &result));
  // The above will hang without the fix.
  EXPECT_EQ(1, result);
  observer.Wait();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// Verifies that the interstitial can go back, even if JavaScript is disabled.
// http://crbug.com/322948
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialJavaScriptGoesBack) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      content::NotificationService::AllSources());
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  content::RenderViewHost* interstitial_rvh =
      interstitial_page->GetMainFrame()->GetRenderViewHost();
  int result = -1;
  const std::string javascript =
      base::StringPrintf("window.domAutomationController.send(%d);",
                         security_interstitials::CMD_DONT_PROCEED);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      interstitial_rvh, javascript, &result));
  // The above will hang without the fix.
  EXPECT_EQ(0, result);
  observer.Wait();
  EXPECT_EQ("about:blank", tab->GetVisibleURL().spec());
}

// Verifies that an overridable interstitial has a proceed link.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialOptionsOverridable) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  content::WaitForInterstitialAttach(tab);
  CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  content::RenderViewHost* interstitial_rvh =
      interstitial_page->GetMainFrame()->GetRenderViewHost();

  content::RenderFrameHost* rfh = interstitial_page->GetMainFrame();
  ASSERT_TRUE(content::WaitForRenderFrameReady(rfh));
  int result = security_interstitials::CMD_ERROR;
  const std::string javascript = base::StringPrintf(
      "domAutomationController.send("
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d))",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(interstitial_rvh, javascript,
                                                  &result));
  EXPECT_EQ(security_interstitials::CMD_TEXT_FOUND, result);
}

// Verifies that a non-overridable interstitial does not have a proceed link.
IN_PROC_BROWSER_TEST_F(SSLUITestHSTS, TestInterstitialOptionsNonOverridable) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL::Replacements replacements;
  replacements.SetHostStr(kHstsTestHostName);
  GURL url = https_server_expired_.GetURL("/ssl/google.html")
                 .ReplaceComponents(replacements);

  ui_test_utils::NavigateToURL(browser(), url);
  content::WaitForInterstitialAttach(tab);
  // Since we are connecting to a different domain than the test server default,
  // we also expect CERT_STATUS_COMMON_NAME_INVALID.
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID | net::CERT_STATUS_COMMON_NAME_INVALID,
      AuthState::SHOWING_INTERSTITIAL);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  content::RenderViewHost* interstitial_rvh =
      interstitial_page->GetMainFrame()->GetRenderViewHost();

  content::RenderFrameHost* rfh = interstitial_page->GetMainFrame();
  ASSERT_TRUE(content::WaitForRenderFrameReady(rfh));
  int result = security_interstitials::CMD_ERROR;
  const std::string javascript = base::StringPrintf(
      "domAutomationController.send("
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d))",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(interstitial_rvh, javascript,
                                                  &result));
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND, result);
}

// Verifies that links in the interstitial open in a new tab.
// https://crbug.com/717616
IN_PROC_BROWSER_TEST_F(SSLUITest, TestInterstitialLinksOpenInNewTab) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* interstitial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  content::WaitForInterstitialAttach(
      browser()->tab_strip_model()->GetActiveWebContents());
  InterstitialPage* interstitial_page = interstitial_tab->GetInterstitialPage();
  ASSERT_TRUE(
      content::WaitForRenderFrameReady(interstitial_page->GetMainFrame()));
  CheckAuthenticationBrokenState(interstitial_tab,
                                 net::CERT_STATUS_DATE_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());

  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();

  SSLBlockingPage* ssl_interstitial =
      static_cast<SSLBlockingPage*>(interstitial_page->GetDelegateForTesting());
  security_interstitials::SecurityInterstitialControllerClient* client =
      GetControllerClientFromInterstitialPage(ssl_interstitial);

  // Mock out the help center URL so that our test will hit the test server
  // instead of a real server.
  // NOTE: The CMD_OPEN_HELP_CENTER code in
  // components/security_interstitials/core/ssl_error_ui.cc ends up appending
  // a path to whatever URL is passed to it. Since that path doesn't exist on
  // our test server, this results in a 404. This is expected behavior, and
  // things are still working as expected so long as the test passes!
  const GURL mock_help_center_url = https_server_.GetURL("/title1.html");
  client->SetBaseHelpCenterUrlForTesting(mock_help_center_url);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  int result = -1;
  const std::string javascript =
      base::StringPrintf("window.domAutomationController.send(%d);",
                         security_interstitials::CMD_OPEN_HELP_CENTER);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      interstitial_page->GetMainFrame(), javascript, &result));
  EXPECT_EQ(security_interstitials::CMD_OPEN_HELP_CENTER, result);

  nav_observer.Wait();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(mock_help_center_url.host(), new_tab->GetURL().host());
}

// Verifies that switching tabs, while showing interstitial page, will not
// affect the visibility of the interstitial.
// https://crbug.com/381439
IN_PROC_BROWSER_TEST_F(SSLUITest, InterstitialNotAffectedByHideShow) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());

  AddTabAtIndex(0, https_server_.GetURL("/ssl/google.html"),
                ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_FALSE(tab->GetRenderWidgetHostView()->IsShowing());

  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
}

// Verifies that if a bad certificate is seen for a host and the user proceeds
// through the interstitial, the decision to proceed is initially remembered.
// However, if this is followed by another visit, and a good certificate
// is seen for the same host, the original exception is forgotten.
IN_PROC_BROWSER_TEST_F(SSLUITest, BadCertFollowedByGoodCert) {
  // It is necessary to use |https_server_expired_| rather than
  // |https_server_mismatched| because the former shares a host with
  // |https_server_| and cert exceptions are per host.
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());

  std::string https_server_expired_host =
      https_server_expired_.GetURL("/ssl/google.html").host();
  std::string https_server_host =
      https_server_.GetURL("/ssl/google.html").host();
  ASSERT_EQ(https_server_expired_host, https_server_host);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  ChromeSSLHostStateDelegate* state =
      reinterpret_cast<ChromeSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  ProceedThroughInterstitial(tab);
  EXPECT_TRUE(state->HasAllowException(https_server_host));

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  ASSERT_FALSE(tab->GetInterstitialPage());
  EXPECT_FALSE(state->HasAllowException(https_server_host));
}

// Tests that the SSLStatus of a navigation entry for an SSL
// interstitial matches the navigation entry once the interstitial is
// clicked through. https://crbug.com/529456
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       SSLStatusMatchesOnInterstitialAndAfterProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));
  content::WaitForInterstitialAttach(tab);
  EXPECT_TRUE(tab->ShowingInterstitialPage());

  content::NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus interstitial_ssl_status = entry->GetSSL();

  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(tab->ShowingInterstitialPage());
  entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);

  content::SSLStatus after_interstitial_ssl_status = entry->GetSSL();
  EXPECT_TRUE(ComparePreAndPostInterstitialSSLStatuses(
      interstitial_ssl_status, after_interstitial_ssl_status));
}

// As above, but for a bad clock interstitial. Tests that a clock
// interstitial's SSLStatus matches the SSLStatus of the HTTPS page
// after proceeding through a normal SSL interstitial.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       SSLStatusMatchesonClockInterstitialAndAfterProceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);

  // Set up the build and current clock times to be more than a year apart.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::NowFromSystemTime());
  mock_clock.Advance(base::TimeDelta::FromDays(367));
  SSLErrorHandler::SetClockForTesting(&mock_clock);
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* clock_interstitial = tab->GetInterstitialPage();
  ASSERT_TRUE(clock_interstitial);
  EXPECT_EQ(BadClockBlockingPage::kTypeForTesting,
            clock_interstitial->GetDelegateForTesting()->GetTypeForTesting());

  // Grab the SSLStatus on the clock interstitial.
  content::NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus clock_interstitial_ssl_status = entry->GetSSL();

  // Put the clock back to normal, trigger a normal SSL interstitial,
  // and proceed through it.
  mock_clock.SetNow(base::Time::NowFromSystemTime());
  ui_test_utils::NavigateToURL(browser(),
                               https_server_expired_.GetURL("/title1.html"));
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* ssl_interstitial = tab->GetInterstitialPage();
  ASSERT_TRUE(ssl_interstitial);
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            ssl_interstitial->GetDelegateForTesting()->GetTypeForTesting());
  ProceedThroughInterstitial(tab);
  EXPECT_FALSE(tab->ShowingInterstitialPage());

  // Grab the SSLStatus from the page and check that it is the same as
  // on the clock interstitial.
  entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus after_interstitial_ssl_status = entry->GetSSL();
  EXPECT_TRUE(ComparePreAndPostInterstitialSSLStatuses(
      clock_interstitial_ssl_status, after_interstitial_ssl_status));
}

// A URLRequestJob that serves valid time server responses, but delays
// them until Resume() is called. If Resume() is called before a request
// is made, then the request will not be delayed.
class DelayableNetworkTimeURLRequestJob : public net::URLRequestJob {
 public:
  DelayableNetworkTimeURLRequestJob(net::URLRequest* request,
                                    net::NetworkDelegate* network_delegate,
                                    bool delayed)
      : net::URLRequestJob(request, network_delegate),
        delayed_(delayed),
        weak_factory_(this) {}

  ~DelayableNetworkTimeURLRequestJob() override {}

  base::WeakPtr<DelayableNetworkTimeURLRequestJob> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // URLRequestJob:
  void Start() override {
    started_ = true;
    if (delayed_) {
      // Do nothing until Resume() is called.
      return;
    }
    Resume();
  }

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    int bytes_read =
        std::min(static_cast<size_t>(buf_size),
                 strlen(network_time::kGoodTimeResponseBody) - data_offset_);
    memcpy(buf->data(), network_time::kGoodTimeResponseBody + data_offset_,
           bytes_read);
    data_offset_ += bytes_read;
    return bytes_read;
  }

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    std::string headers;
    headers.append(
        "HTTP/1.1 200 OK\n"
        "Content-type: text/plain\n");
    headers.append(base::StringPrintf(
        "Content-Length: %1d\n",
        static_cast<int>(strlen(network_time::kGoodTimeResponseBody))));
    info->headers =
        new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
            headers.c_str(), static_cast<int>(headers.length())));
    info->headers->AddHeader(
        "x-cup-server-proof: " +
        std::string(network_time::kGoodTimeResponseServerProofHeader));
  }

  // Resumes a previously started request that was delayed. If no
  // request has been started yet, then when Start() is called it will
  // not delay.
  void Resume() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(delayed_);
    if (!started_) {
      // If Start() hasn't been called yet, then unset |delayed_| so
      // that when Start() is called, the request will begin
      // immediately.
      delayed_ = false;
      return;
    }

    // Start reading asynchronously as would a normal network request.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DelayableNetworkTimeURLRequestJob::NotifyHeadersComplete,
            weak_factory_.GetWeakPtr()));
  }

 private:
  bool delayed_;
  bool started_ = false;
  int data_offset_ = 0;
  base::WeakPtrFactory<DelayableNetworkTimeURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelayableNetworkTimeURLRequestJob);
};

// A URLRequestInterceptor that intercepts requests to use
// DelayableNetworkTimeURLRequestJobs. Expects to intercept only a
// single request in its lifetime.
class DelayedNetworkTimeInterceptor : public net::URLRequestInterceptor {
 public:
  DelayedNetworkTimeInterceptor() {}
  ~DelayedNetworkTimeInterceptor() override {}

  // Intercepts |request| to use a DelayableNetworkTimeURLRequestJob. If
  // Resume() has been called before MaybeInterceptRequest(), then the
  // request will not be delayed.
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    // Only support one intercepted request.
    EXPECT_FALSE(intercepted_request_);
    intercepted_request_ = true;
    // If the request has been resumed before this request is created,
    // then |should_delay_requests_| will be false and the request will
    // not delay.
    DelayableNetworkTimeURLRequestJob* job =
        new DelayableNetworkTimeURLRequestJob(request, network_delegate,
                                              should_delay_requests_);
    if (should_delay_requests_)
      delayed_request_ = job->GetWeakPtr();
    return job;
  }

  void Resume() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!should_delay_requests_)
      return;
    should_delay_requests_ = false;
    if (delayed_request_)
      delayed_request_->Resume();
  }

 private:
  // True if a request has been intercepted. Used to enforce that only
  // one request is intercepted in this object's lifetime.
  mutable bool intercepted_request_ = false;
  // True until Resume() is called. If Resume() is called before a
  // request is intercepted, then a request that is intercepted later
  // will continue without a delay.
  bool should_delay_requests_ = true;
  // Use a WeakPtr in case the request is cancelled before Resume() is called.
  mutable base::WeakPtr<DelayableNetworkTimeURLRequestJob> delayed_request_ =
      nullptr;

  DISALLOW_COPY_AND_ASSIGN(DelayedNetworkTimeInterceptor);
};

// IO-thread helper methods for SSLNetworkTimeBrowserTest.

void ResumeDelayedNetworkTimeRequest(
    DelayedNetworkTimeInterceptor* interceptor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  interceptor->Resume();
}

void SetUpNetworkTimeInterceptorOnIOThread(
    DelayedNetworkTimeInterceptor* interceptor,
    const GURL& time_server_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      time_server_url.scheme(), time_server_url.host(),
      std::unique_ptr<DelayedNetworkTimeInterceptor>(interceptor));
}

void CleanUpOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter::GetInstance()->ClearHandlers();
}

// A fixture for testing on-demand network time queries on SSL
// certificate date errors. It can simulate a delayed network time
// request, and it allows the user to configure the experimental
// parameters of the NetworkTimeTracker. Expects only one network time
// request to be issued during the test.
class SSLNetworkTimeBrowserTest : public SSLUITest {
 public:
  SSLNetworkTimeBrowserTest() : SSLUITest(), interceptor_(nullptr) {}
  ~SSLNetworkTimeBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        "SSLNetworkTimeBrowserTestFieldTrial/Enabled/");
    command_line->AppendSwitchASCII(
        variations::switches::kForceFieldTrialParams,
        "SSLNetworkTimeBrowserTestFieldTrial.Enabled:FetchBehavior/"
        "on-demand-only");
  }

  void SetUpOnMainThread() override {
    SSLUITest::SetUpOnMainThread();
    scoped_feature_list_.InitFromCommandLine(
        std::string(network_time::kNetworkTimeServiceQuerying.name) +
            "<SSLNetworkTimeBrowserTestFieldTrial",
        std::string());
    SetUpNetworkTimeServer();
  }

  void TearDownOnMainThread() override {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     base::BindOnce(&CleanUpOnIOThread));
  }

 protected:
  void SetUpNetworkTimeServer() {
    // Install the URL interceptor that serves delayed network time responses.
    interceptor_ = new DelayedNetworkTimeInterceptor();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&SetUpNetworkTimeInterceptorOnIOThread,
                       base::Unretained(interceptor_),
                       g_browser_process->network_time_tracker()
                           ->GetTimeServerURLForTesting()));
  }

  void TriggerTimeResponse() {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ResumeDelayedNetworkTimeRequest,
                       base::Unretained(interceptor_)));
  }

  // Asserts that the first time request to the server is currently pending.
  void CheckTimeQueryPending() {
    base::Time unused_time;
    base::TimeDelta unused_uncertainty;
    ASSERT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING,
              g_browser_process->network_time_tracker()->GetNetworkTime(
                  &unused_time, &unused_uncertainty));
  }

 private:
  DelayedNetworkTimeInterceptor* interceptor_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLNetworkTimeBrowserTest);
};

// Tests that if an on-demand network time fetch returns that the clock
// is okay, a normal SSL interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, OnDemandFetchClockOk) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Use a testing clock set to the time that GoodTimeResponseHandler
  // returns, to simulate the system clock matching the network time.
  base::SimpleTestClock testing_clock;
  SSLErrorHandler::SetClockForTesting(&testing_clock);
  testing_clock.SetNow(
      base::Time::FromJsTime(network_time::kGoodTimeResponseHandlerJsTime));
  // Set the build time to match the testing clock, to ensure that the
  // build time heuristic doesn't fire.
  ssl_errors::SetBuildTimeForTesting(testing_clock.Now());

  // Set a long timeout to ensure that the on-demand time fetch completes.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Once |interstitial_timer_observer| has fired, the request has been
  // sent. Override the nonce that NetworkTimeTracker expects so that
  // when the response comes back, it will validate. The nonce can only
  // be overriden for the current in-flight request, so the test must
  // call OverrideNonceForTesting() after the request has been sent and
  // before the response has been received.
  interstitial_timer_observer.WaitForTimerStarted();
  g_browser_process->network_time_tracker()->OverrideNonceForTesting(123123123);
  TriggerTimeResponse();

  EXPECT_TRUE(contents->IsLoading());
  observer.Wait();
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());
  InterstitialPage* interstitial_page = contents->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
}

// Tests that if an on-demand network time fetch returns that the clock
// is wrong, a bad clock interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, OnDemandFetchClockWrong) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Use a testing clock set to a time that is different from what
  // GoodTimeResponseHandler returns, simulating a system clock that is
  // 30 days ahead of the network time.
  base::SimpleTestClock testing_clock;
  SSLErrorHandler::SetClockForTesting(&testing_clock);
  testing_clock.SetNow(
      base::Time::FromJsTime(network_time::kGoodTimeResponseHandlerJsTime));
  testing_clock.Advance(base::TimeDelta::FromDays(30));
  // Set the build time to match the testing clock, to ensure that the
  // build time heuristic doesn't fire.
  ssl_errors::SetBuildTimeForTesting(testing_clock.Now());

  // Set a long timeout to ensure that the on-demand time fetch completes.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  // Once |interstitial_timer_observer| has fired, the request has been
  // sent. Override the nonce that NetworkTimeTracker expects so that
  // when the response comes back, it will validate. The nonce can only
  // be overriden for the current in-flight request, so the test must
  // call OverrideNonceForTesting() after the request has been sent and
  // before the response has been received.
  interstitial_timer_observer.WaitForTimerStarted();
  g_browser_process->network_time_tracker()->OverrideNonceForTesting(123123123);
  TriggerTimeResponse();

  EXPECT_TRUE(contents->IsLoading());
  observer.Wait();
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());
  InterstitialPage* interstitial_page = contents->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(BadClockBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
}

// Tests that if the timeout expires before the network time fetch
// returns, then a normal SSL interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       TimeoutExpiresBeforeFetchCompletes) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to fire immediately.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());
  InterstitialPage* interstitial_page = contents->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());

  // Navigate away, and then trigger the network time response; no crash should
  // occur.
  ASSERT_TRUE(https_server_.Start());
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user stops the page load before either the network
// time fetch completes or the timeout expires, then there is no interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, StopBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  contents->Stop();
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response; no crash should
  // occur.
  ASSERT_TRUE(https_server_.Start());
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/title1.html"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user reloads the page before either the network
// time fetch completes or the timeout expires, then there is no interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest, ReloadBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response and wait
  // for the response; no crash should occur.
  ASSERT_TRUE(https_server_.Start());
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user navigates away before either the network time
// fetch completes or the timeout expires, then there is no
// interstitial.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       NavigateAwayBeforeTimeoutExpires) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_TRUE(https_server_.Start());
  // Set the timer to a long delay.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_expired_.GetURL("/"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(content::OpenURLParams(
      https_server_.GetURL("/"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(contents));
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());

  // Navigate away, and then trigger the network time response and wait
  // for the response; no crash should occur.
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL("/"));
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

// Tests that if the user closes the tab before the network time fetch
// completes, it doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(SSLNetworkTimeBrowserTest,
                       CloseTabBeforeNetworkFetchCompletes) {
  ASSERT_TRUE(https_server_expired_.Start());
  // Set the timer to fire immediately.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  content::WaitForInterstitialAttach(contents);

  EXPECT_TRUE(contents->ShowingInterstitialPage());
  InterstitialPage* interstitial_page = contents->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());

  // Open a second tab, close the first, and then trigger the network time
  // response and wait for the response; no crash should occur.
  ASSERT_TRUE(https_server_.Start());
  AddTabAtIndex(1, https_server_.GetURL("/"), ui::PAGE_TRANSITION_TYPED);
  chrome::CloseWebContents(browser(), contents, false);
  ASSERT_NO_FATAL_FAILURE(CheckTimeQueryPending());
  TriggerTimeResponse();
}

namespace {

// Fails with a CHECK for all requests over HTTP except for favicons. This is to
// ensure that name mismatch redirect feature's suggest URL ping stops on
// redirects and never hits an HTTP URL.
class HttpNameMismatchPingInterceptor : public net::URLRequestInterceptor {
 public:
  HttpNameMismatchPingInterceptor() {}
  ~HttpNameMismatchPingInterceptor() override {}

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* delegate) const override {
    if (request->url().path() == "/favicon.ico") {
      // When a page doesn't list a favicon, a favicon request is automatically
      // made over HTTP. These are harmless and don't leak the original page's
      // URL, so ignore them.
      return nullptr;
    }

    EXPECT_TRUE(false)
        << "Name mismatch pings must never be over HTTP. This request was for "
        << request->url();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpNameMismatchPingInterceptor);
};

void SetUpHttpNameMismatchPingInterceptorOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Add interceptors for HTTP versions of example.org and www.example.org.
  // These are the hostnames used in the tests, and we never want them to be
  // contacted over HTTP.
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      "http", "example.org",
      std::unique_ptr<HttpNameMismatchPingInterceptor>(
          new HttpNameMismatchPingInterceptor()));
  net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
      "http", "www.example.org",
      std::unique_ptr<HttpNameMismatchPingInterceptor>(
          new HttpNameMismatchPingInterceptor()));
}

}  // namespace

class CommonNameMismatchBrowserTest : public CertVerifierBrowserTest {
 public:
  CommonNameMismatchBrowserTest() : CertVerifierBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable finch experiment for SSL common name mismatch handling.
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "SSLCommonNameMismatchHandling/Enabled/");
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&SetUpHttpNameMismatchPingInterceptorOnIOThread));
  }

  void TearDownOnMainThread() override {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     base::BindOnce(&CleanUpOnIOThread));
    CertVerifierBrowserTest::TearDownOnMainThread();
  }
};

// Visit the URL www.mail.example.com on a server that presents a valid
// certificate for mail.example.com. Verify that the page navigates to
// mail.example.com.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       ShouldShowWWWSubdomainMismatchInterstitial) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  // Use the "spdy_pooling.pem" cert which has "mail.example.com"
  // as one of its SANs.
  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  // Request to "www.mail.example.com" should result in
  // |net::ERR_CERT_COMMON_NAME_INVALID| error.
  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  // Request to "www.mail.example.com" should not result in any error.
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  // Use a complex URL to ensure the path, etc., are preserved. The path itself
  // does not matter.
  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b#anchor");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(
      contents,
      // With PlzNavigate, the renderer only sees one navigation (i.e. not the
      // redirect, since that happens in the browser).
      content::IsBrowserSideNavigationEnabled() ? 1 : 2);
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  observer.Wait();

  CheckSecurityState(contents, CertError::NONE, security_state::SECURE,
                     AuthState::NONE);
  replacements.SetHostStr("mail.example.com");
  GURL https_server_new_url = https_server_url.ReplaceComponents(replacements);
  // Verify that the current URL is the suggested URL.
  EXPECT_EQ(https_server_new_url.spec(),
            contents->GetLastCommittedURL().spec());
}

// Visit the URL example.org on a server that presents a valid certificate
// for www.example.org. Verify that the page redirects to www.example.org.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       CheckWWWSubdomainMismatchInverse) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "example.org", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "www.example.org",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(
      contents,
      // With PlzNavigate, the renderer only sees one navigation (i.e. not the
      // redirect, since that happens in the browser).
      content::IsBrowserSideNavigationEnabled() ? 1 : 2);
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  observer.Wait();

  CheckSecurityState(contents, CertError::NONE, security_state::SECURE,
                     AuthState::NONE);
}

namespace {
// Redirects incoming request to http://example.org.
std::unique_ptr<net::test_server::HttpResponse> HTTPSToHTTPRedirectHandler(
    const net::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  replacements.SetSchemeStr("http");
  const GURL redirect_url =
      test_server->base_url().ReplaceComponents(replacements);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return std::move(http_response);
}
}  // namespace

// Common name mismatch handling feature should ignore redirects when pinging
// the suggested hostname. Visit the URL example.org on a server that presents a
// valid certificate for www.example.org. In this case, www.example.org
// redirects to http://example.org, and the SSL error should not be redirected
// to this URL.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       WWWSubdomainMismatch_StopOnRedirects) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);

  // Redirect all URLs to http://example.org. Since this test will trigger only
  // one request to check the suggested URL, redirecting all requests is OK.
  // We would normally use content::SetupCrossSiteRedirector here, but that
  // function does not support https to http redirects.
  // This must be done before ServeFilesFromSourceDirectory(), otherwise the
  // test server will serve files instead of redirecting requests to them.
  https_server_example_domain.RegisterRequestHandler(
      base::Bind(&HTTPSToHTTPRedirectHandler, &https_server_example_domain));

  https_server_example_domain.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));

  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "example.org", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "www.example.org",
                                                verify_result_valid, net::OK);

  // The user will visit https://example.org:port/ssl/blank.html.
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  const GURL https_server_mismatched_url =
      https_server_example_domain.GetURL("/ssl/blank.html")
          .ReplaceComponents(replacements);

  // Should simply show an interstitial, because the suggested URL
  // (https://www.example.org) redirected to http://example.org.
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  WaitForInterstitialAttach(contents);

  CheckSecurityState(contents, net::CERT_STATUS_COMMON_NAME_INVALID,
                     security_state::DANGEROUS,
                     AuthState::SHOWING_INTERSTITIAL);
}

// Tests this scenario:
// - |CommonNameMismatchHandler| does not give a callback as it's set into the
//   state |IGNORE_REQUESTS_FOR_TESTING|. So no suggested URL check result can
//   arrive.
// - A cert error triggers an interstitial timer with a very long timeout.
// - No suggested URL check results arrive, causing the tab to appear as loading
//   indefinitely (also because the timer has a long timeout).
// - Stopping the page load shouldn't result in any interstitials.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialStopNavigationWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  contents->Stop();
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());
}

// Same as above, but instead of stopping, the loading page is reloaded. The end
// result is the same. (i.e. page load stops, no interstitials shown)
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialReloadNavigationWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());
}

// Same as above, but instead of reloading, the page is navigated away. The
// new page should load, and no interstitials should be shown.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       InterstitialNavigateAwayWhileLoading) {
  net::EmbeddedTestServer https_server_example_domain(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain.Start());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain.GetCertificate();

  net::CertVerifyResult verify_result;
  verify_result.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  verify_result.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  mock_cert_verifier()->AddResultForCertAndHost(
      cert.get(), "www.mail.example.com", verify_result,
      net::ERR_CERT_COMMON_NAME_INVALID);

  net::CertVerifyResult verify_result_valid;
  verify_result_valid.verified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "spdy_pooling.pem");
  mock_cert_verifier()->AddResultForCertAndHost(cert.get(), "mail.example.com",
                                                verify_result_valid, net::OK);

  const GURL https_server_url =
      https_server_example_domain.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  const GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url,
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(content::OpenURLParams(
      GURL("https://google.com"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  observer.Wait();

  SSLErrorHandler* ssl_error_handler =
      SSLErrorHandler::FromWebContents(contents);
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(ssl_error_handler);
  EXPECT_FALSE(contents->ShowingInterstitialPage());
  EXPECT_FALSE(contents->IsLoading());
}

class SSLBlockingPageIDNTest : public SecurityInterstitialIDNTest {
 protected:
  // SecurityInterstitialIDNTest implementation
  security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo ssl_info;
    ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    return SSLBlockingPage::Create(
        contents, net::ERR_CERT_CONTAINS_ERRORS, ssl_info, request_url, 0,
        base::Time::NowFromSystemTime(), nullptr, false /* is superfish */,
        base::Callback<void(content::CertificateRequestResultType)>());
  }
};

// Flaky on mac: https://crbug.com/689846
#if defined(OS_MACOSX)
#define MAYBE_SSLBlockingPageDecodesIDN DISABLED_SSLBlockingPageDecodesIDN
#else
#define MAYBE_SSLBlockingPageDecodesIDN SSLBlockingPageDecodesIDN
#endif

IN_PROC_BROWSER_TEST_F(SSLBlockingPageIDNTest,
                       MAYBE_SSLBlockingPageDecodesIDN) {
  EXPECT_TRUE(VerifyIDNDecoded());
}

IN_PROC_BROWSER_TEST_F(CertVerifierBrowserTest, MockCertVerifierSmokeTest) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server.Start());

  mock_cert_verifier()->set_default_result(
      net::ERR_CERT_NAME_CONSTRAINT_VIOLATION);

  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("/ssl/google.html"));

  CheckSecurityState(browser()->tab_strip_model()->GetActiveWebContents(),
                     net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION,
                     security_state::DANGEROUS,
                     AuthState::SHOWING_INTERSTITIAL);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, RestoreHasSSLState) {
  ASSERT_TRUE(https_server_.Start());
  GURL url(https_server_.GetURL("/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  NavigationEntry* entry = tab->GetController().GetLastCommittedEntry();
  std::unique_ptr<NavigationEntry> restored_entry =
      content::NavigationController::CreateNavigationEntry(
          url, content::Referrer(), ui::PAGE_TRANSITION_RELOAD, false,
          std::string(), tab->GetBrowserContext());
  restored_entry->SetPageState(entry->GetPageState());

  WebContents::CreateParams params(tab->GetBrowserContext());
  WebContents* tab2 = WebContents::Create(params);
  tab->GetDelegate()->AddNewContents(
      nullptr, tab2, WindowOpenDisposition::NEW_FOREGROUND_TAB, gfx::Rect(),
      false, nullptr);
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  content::TestNavigationObserver observer(tab2);
  tab2->GetController().Restore(
      entries.size() - 1,
      content::RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  tab2->GetController().LoadIfNecessary();
  observer.Wait();
  CheckAuthenticatedState(tab2, AuthState::NONE);
}

void SetupRestoredTabWithNavigation(
    net::test_server::EmbeddedTestServer* https_server,
    Browser* browser) {
  ASSERT_TRUE(https_server->Start());
  GURL url(https_server->GetURL("/ssl/google.html"));
  ui_test_utils::NavigateToURL(browser, url);
  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(tab);
  EXPECT_TRUE(ExecuteScript(tab, "history.pushState({}, '', '');"));
  observer.Wait();

  ui_test_utils::NavigateToURLWithDisposition(
      browser, GURL("about:blank"), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  chrome::CloseTab(browser);

  WebContents* blank_tab = browser->tab_strip_model()->GetActiveWebContents();

  // Restore the tab.
  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());
  content::WindowedNotificationObserver tab_loaded_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::RestoreTab(browser);
  tab_added_observer.Wait();
  tab_loaded_observer.Wait();

  tab = browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(tab, blank_tab);
}

// Simulate a browser-initiated in-page navigation in a restored tab.
// https://crbug.com/662267
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       BrowserInitiatedExistingPageAfterRestoreHasSSLState) {
  SetupRestoredTabWithNavigation(&https_server_, browser());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate a renderer-initiated in-page navigation in a restored tab.
IN_PROC_BROWSER_TEST_F(SSLUITest,
                       RendererInitiatedExistingPageAfterRestoreHasSSLState) {
  SetupRestoredTabWithNavigation(&https_server_, browser());
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab);
  ASSERT_TRUE(content::ExecuteScript(
      tab, "location.replace(window.location.href + '#1')"));
  observer.Wait();
  content::WaitForLoadStop(tab);
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that a restore followed immediately by a history navigation doesn't
// lose SSL state.
// Disabled since this is a test for bug 738177.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_RestoreThenNavigateHasSSLState) {
  // This race condition only happens with PlzNavigate.
  if (!content::IsBrowserSideNavigationEnabled())
    return;
  ASSERT_TRUE(https_server_.Start());
  GURL url1(https_server_.GetURL("/ssl/google.html"));
  GURL url2(https_server_.GetURL("/ssl/page_with_refs.html"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURL(browser(), url2);
  chrome::CloseTab(browser());

  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_PARENTED,
      content::NotificationService::AllSources());
  chrome::RestoreTab(browser());
  tab_added_observer.Wait();

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationManager observer(tab, url1);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.WaitForNavigationFinished();
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Simulate the URL changing when the user presses enter in the omnibox. This
// could happen when the user's login is expired and the server redirects them
// to a login page. This will be considered a same document navigation but we
// do want to update the SSL state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentHasSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to a simple page and then perform an in-page navigation.
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), start_url);

  GURL fragment_change_url(embedded_test_server()->GetURL("/title1.html#foo"));
  ui_test_utils::NavigateToURL(browser(), fragment_change_url);
  CheckUnauthenticatedState(tab, AuthState::NONE);

  // Replace the URL of the current NavigationEntry with one that will cause
  // a server redirect when loaded.
  {
    GURL redirect_dest_url(https_server_.GetURL("/ssl/google.html"));
    content::TestNavigationObserver observer(tab);
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(tab, script));
    observer.Wait();
  }

  // Simulate the user hitting Enter in the omnibox without changing the URL.
  {
    content::TestNavigationObserver observer(tab);
    tab->GetController().LoadURL(tab->GetLastCommittedURL(),
                                 content::Referrer(), ui::PAGE_TRANSITION_LINK,
                                 std::string());
    observer.Wait();
  }

  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading, the SSL state
// reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL https_url = https_server_.GetURL("/ssl/redirect.html?");
  GURL http_url = embedded_test_server()->GetURL("/ssl/google.html");

  GURL url(https_url.spec() + http_url.spec());
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading from a mixed
// content to a valid HTTPS page, the SSL state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectFromMixedContentSSLState) {
  ASSERT_TRUE(https_server_.Start());

  GURL url =
      GURL(https_server_.GetURL("/ssl/redirect_with_mixed_content.html").spec()
      + "?" +
      https_server_.GetURL("/ssl/google.html").spec());

  // Load a page that displays insecure content.
  ui_test_utils::NavigateToURL(browser(), url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that if a redirect occurs while the page is loading from a valid HTTPS
// page to a mixed content page, the SSL state reflects the final URL.
IN_PROC_BROWSER_TEST_F(SSLUITest, ClientRedirectToMixedContentSSLState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  GURL url =
      GURL(https_server_.GetURL("/ssl/redirect.html").spec()
      + "?" +
      https_server_.GetURL("/ssl/page_displays_insecure_content.html").spec());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url, 2);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckSecurityState(tab, CertError::NONE, security_state::NONE,
                     AuthState::DISPLAYED_INSECURE_CONTENT);
}

// Checks that same-document navigations during page load preserve SSL state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentNavigationDuringLoadSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(
      browser(),
      https_server_.GetURL("/ssl/same_document_navigation_during_load.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that same-document navigations after the page load preserve SSL
// state.
IN_PROC_BROWSER_TEST_F(SSLUITest, SameDocumentNavigationAfterLoadSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(tab, "location.hash = Math.random()"));
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Checks that navigations after pushState maintain the SSL status.
IN_PROC_BROWSER_TEST_F(SSLUITest, PushStateSSLState) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticatedState(tab, AuthState::NONE);

  content::TestNavigationObserver observer(tab);
  EXPECT_TRUE(ExecuteScript(tab, "history.pushState({}, '', '');"));
  observer.Wait();
  CheckAuthenticatedState(tab, AuthState::NONE);

  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(tab);
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// Regression test for http://crbug.com/635833 (crash when a window with no
// NavigationEntry commits).
IN_PROC_BROWSER_TEST_F(SSLUITestIgnoreLocalhostCertErrors,
                       NoCrashOnLoadWithNoNavigationEntry) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(tab, "window.open()"));
}

// Put captive portal related tests under a different namespace for nicer
// pattern matching.
using SSLUICaptivePortalListTest = SSLUITest;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
MakeCaptivePortalConfig(int version_id,
                        const std::set<std::string>& spki_hashes) {
  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(version_id);
  for (const std::string& hash : spki_hashes) {
    config_proto->add_captive_portal_cert()->set_sha256_hash(hash);
  }
  return config_proto;
}

// Tests that the captive portal certificate list is not used when the feature
// is disabled via Finch. The list is passed to SSLErrorHandler via a proto.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListTest, Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Use InitFromCommandLine instead of InitAndDisableFeature to avoid making
  // the feature public in SSLErrorHandler header.
  scoped_feature_list.InitFromCommandLine(
      std::string() /* enabled */,
      "CaptivePortalCertificateList" /* disabled */);

  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  const net::HashValue server_spki_hash =
      GetSPKIHash(https_server_mismatched_.GetCertificate().get());
  SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
      kLargeVersionId, std::set<std::string>{server_spki_hash.ToString()}));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. A normal SSL interstitial should
  // be displayed since CaptivePortalCertificateList feature is disabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  content::WaitForInterstitialAttach(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the SSL interstitial was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 0);
}

// Tests that the captive portal certificate list is used when the feature
// is enabled via Finch. The list is passed to SSLErrorHandler via a proto.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListTest, Enabled_FromProto) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "CaptivePortalCertificateList" /* enabled */,
      std::string() /* disabled */);

  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  const net::HashValue server_spki_hash =
      GetSPKIHash(https_server_mismatched_.GetCertificate().get());
  SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
      kLargeVersionId, std::set<std::string>{server_spki_hash.ToString()}));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. The captive portal interstitial
  // should be displayed since CaptivePortalCertificateList feature is enabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  content::WaitForInterstitialAttach(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
}

// Tests the scenario where the OS reports a captive portal. A captive portal
// interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(SSLUITest, OSReportsCaptivePortal) {
  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  // Navigate to an unsafe page on the server.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  content::WaitForInterstitialAttach(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 1);
}

// Tests the scenario where the OS reports a captive portal but captive portal
// interstitial feature is disabled. A captive portal interstitial should not be
// displayed.
IN_PROC_BROWSER_TEST_F(SSLUITest, OSReportsCaptivePortal_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      std::string() /* enabled */,
      std::string("CaptivePortalInterstitial") /* disabled */);

  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  // Navigate to an unsafe page on the server.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  content::WaitForInterstitialAttach(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the SSL interstitial was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 0);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 0);
}

namespace {

// SPKI hash to captive-portal.badssl.com leaf certificate. This
// doesn't match the actual cert (ok_cert.pem) but is good enough for testing.
const char kCaptivePortalSPKI[] =
    "sha256/fjZPHewEHTrMDX3I1ecEIeoy3WFxHyGplOLv28kIbtI=";

// Test class that mimics a URL request with a certificate whose SPKI hash is in
// ssl_error_assistant.asciipb resource. A better way of testing the SPKI hashes
// inside the resource bundle would be to serve the actual certificate from the
// embedded test server, but the test server can only serve a limited number of
// predefined certificates.
class SSLUICaptivePortalListResourceBundleTest
    : public CertVerifierBrowserTest {
 public:
  SSLUICaptivePortalListResourceBundleTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  }

  void SetUp() override {
    CertVerifierBrowserTest::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
    SetUpCertVerifier(0, net::OK, std::string());
  }

  void TearDown() override {
    SSLErrorHandler::ResetConfigForTesting();
    CertVerifierBrowserTest::TearDown();
  }

 protected:
  // Checks that a captive portal interstitial isn't displayed, even though the
  // server's certificate is marked as a captive portal certificate.
  void TestNoCaptivePortalInterstitial(net::CertStatus cert_status,
                                       int net_error) {
    ASSERT_TRUE(https_server()->Start());
    base::HistogramTester histograms;

    // Mark the server's cert as a captive portal cert.
    SetUpCertVerifier(cert_status, net_error, kCaptivePortalSPKI);

    // Navigate to an unsafe page on the server. CaptivePortalCertificateList
    // feature is enabled but either the error is not name-mismatch, or it's not
    // the only error, so a generic SSL interstitial should be displayed.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    content::WaitForInterstitialAttach(tab);

    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that the histogram for the captive portal cert was recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }

  void SetUpCertVerifier(net::CertStatus cert_status,
                         int net_result,
                         const std::string& spki_hash) {
    scoped_refptr<net::X509Certificate> cert(https_server_.GetCertificate());
    net::CertVerifyResult verify_result;
    verify_result.is_issued_by_known_root =
        (net_result != net::ERR_CERT_AUTHORITY_INVALID);
    verify_result.verified_cert = cert;
    verify_result.cert_status = cert_status;

    // Set the SPKI hash to captive-portal.badssl.com leaf certificate.
    if (!spki_hash.empty()) {
      net::HashValue hash;
      ASSERT_TRUE(hash.FromString(spki_hash));
      verify_result.public_key_hashes.push_back(hash);
    }
    mock_cert_verifier()->AddResultForCert(cert, verify_result, net_result);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
};

}  // namespace

// Same as CaptivePortalCertificateList_Enabled_FromProto, but this time the
// cert's SPKI hash is listed in ssl_error_assistant.asciipb.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest, Enabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "CaptivePortalCertificateList" /* enabled */,
      std::string() /* disabled */);
  ASSERT_TRUE(https_server()->Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID,
                    net::ERR_CERT_COMMON_NAME_INVALID, kCaptivePortalSPKI);

  // Navigate to an unsafe page on the server. The captive portal interstitial
  // should be displayed since CaptivePortalCertificateList feature is enabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
  content::WaitForInterstitialAttach(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
}

// Same as SSLUICaptivePortalListResourceBundleTest. Enabled, but this time the
// proto is dynamically updated (e.g. by the component updater). The dynamic
// update should always override the proto loaded from the resource bundle.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest,
                       Enabled_DynamicUpdate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "CaptivePortalCertificateList" /* enabled */,
      std::string() /* disabled */);
  ASSERT_TRUE(https_server()->Start());

  // Mark the server's cert as a captive portal cert.
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID,
                    net::ERR_CERT_COMMON_NAME_INVALID, kCaptivePortalSPKI);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  {
    // Dynamically update the SSL error assistant config, do not include the
    // captive portal SPKI hash.
    SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
        kLargeVersionId,
        std::set<std::string>{"sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. A generic SSL interstitial
    // should be displayed because the dynamic update doesn't contain the hash
    // of the captive portal certificate.
    base::HistogramTester histograms;
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    content::WaitForInterstitialAttach(tab);

    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that the histogram was recorded for an SSL interstitial.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }
  {
    // Dynamically update the error assistant config and add the captive portal
    // SPKI hash.
    SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
        kLargeVersionId + 1,
        std::set<std::string>{"sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                              kCaptivePortalSPKI}));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to the unsafe page again. This time, a captive portal
    // interstitial should be displayed.
    base::HistogramTester histograms;
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    content::WaitForInterstitialAttach(tab);

    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histogram was recorded for a captive portal interstitial.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                3);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
  }
  {
    // Try dynamically updating the error assistant config with an empty config
    // with the same version number. The update should be ignored, and a captive
    // portal interstitial should still be displayed.
    SSLErrorHandler::SetErrorAssistantProto(
        MakeCaptivePortalConfig(kLargeVersionId + 1, std::set<std::string>()));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to the unsafe page again. This time, a captive portal
    // interstitial should be displayed.
    base::HistogramTester histograms;
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), https_server()->GetURL("/"));
    content::WaitForInterstitialAttach(tab);

    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histogram was recorded for a captive portal interstitial.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                3);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::CAPTIVE_PORTAL_CERT_FOUND, 1);
  }
}

// Same as SSLUICaptivePortalNameMismatchTest, but this time the error is
// authority-invalid. Captive portal interstitial should not be shown.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest,
                       Enabled_AuthorityInvalid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "CaptivePortalCertificateList" /* enabled */,
      std::string() /* disabled */);

  TestNoCaptivePortalInterstitial(net::CERT_STATUS_AUTHORITY_INVALID,
                                  net::ERR_CERT_AUTHORITY_INVALID);
}

// Same as SSLUICaptivePortalListResourceBundleTest.Enabled_AuthorityInvalid,
// but this time there are two errors (name mismatch + weak key). Captive portal
// interstitial should not be shown when name mismatch isn't the only error.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListResourceBundleTest,
                       Enabled_NameMismatchAndWeakKey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "CaptivePortalCertificateList" /* enabled */,
      std::string() /* disabled */);

  const net::CertStatus cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_WEAK_KEY;
  // Sanity check that COMMON_NAME_INVALID is seen as the net error, since the
  // test is designed to verify that SSLErrorHandler notices other errors in the
  // CertStatus even when COMMON_NAME_INVALID is the net error.
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(cert_status));
  TestNoCaptivePortalInterstitial(cert_status,
                                  net::ERR_CERT_COMMON_NAME_INVALID);
}

#else

// Tests that the captive portal certificate list is not used when captive
// portal checks are disabled by build, even if the captive portal certificate
// list feature is enabled via Finch. The list is passed to SSLErrorHandler via
// a proto.
IN_PROC_BROWSER_TEST_F(SSLUICaptivePortalListTest, PortalChecksDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "CaptivePortalCertificateList" /* enabled */,
      std::string() /* disabled */);

  ASSERT_TRUE(https_server_mismatched_.Start());
  base::HistogramTester histograms;

  // Mark the server's cert as a captive portal cert.
  SSLErrorHandler::SetErrorAssistantProto(MakeCaptivePortalConfig(
      kLargeVersionId + 1, std::set<std::string>{server_spki_hash.ToString()}));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. The captive portal interstitial
  // should be displayed since CaptivePortalCertificateList feature is enabled.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html"));
  content::WaitForInterstitialAttach(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_FALSE(interstitial_timer_observer.timer_started());

  // Check that the histogram for the captive portal cert was recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

namespace {

char kTestMITMSoftwareName[] = "Misconfigured Firewall";

class SSLUIMITMSoftwareTest : public CertVerifierBrowserTest {
 public:
  SSLUIMITMSoftwareTest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SSLUIMITMSoftwareTest() override {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &SetHSTSForHostName,
            base::RetainedRef(browser()->profile()->GetRequestContext())));
  }

  // Set up the cert verifier to return the error passed in as the cert_error
  // parameter.
  void SetUpCertVerifier(net::CertStatus cert_error) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(verify_result.verified_cert);

    verify_result.cert_status = cert_error;
    mock_cert_verifier()->AddResultForCert(
        https_server()->GetCertificate().get(), verify_result,
        net::MapCertStatusToNetError(cert_error));
  }

  // Sets up an SSLErrorAssistantProto that lists |https_server_|'s default
  // certificate as a MITM software certificate.
  void SetUpMITMSoftwareCertList(uint32_t version_id) {
    auto config_proto =
        base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(version_id);

    chrome_browser_ssl::MITMSoftware* mitm_software =
        config_proto->add_mitm_software();
    mitm_software->set_name(kTestMITMSoftwareName);
    mitm_software->set_issuer_common_name_regex(
        https_server()->GetCertificate().get()->issuer().common_name);
    mitm_software->set_issuer_organization_regex(
        https_server()->GetCertificate().get()->issuer().organization_names[0]);
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);
  }

  // Returns a URL which triggers an interstitial with the host name that has
  // HSTS set.
  GURL GetHSTSTestURL() const {
    GURL::Replacements replacements;
    replacements.SetHostStr(kHstsTestHostName);
    return https_server()
        ->GetURL("/ssl/blank_page.html")
        .ReplaceComponents(replacements);
  }

  void TestMITMSoftwareInterstitial() {
    base::HistogramTester histograms;
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. Mock out the URL host name to
    // equal the one set for HSTS.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
    content::WaitForInterstitialAttach(tab);
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(MITMSoftwareBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    EXPECT_FALSE(interstitial_timer_observer.timer_started());

    // Check that the histograms for the MITM software interstitial were
    // recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 1);
  }

  void TestNoMITMSoftwareInterstitial() {
    base::HistogramTester histograms;
    ASSERT_TRUE(https_server()->Start());
    ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() >
                0);

    // Navigate to an unsafe page on the server. Mock out the URL host name to
    // equal the one set for HSTS.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    SSLInterstitialTimerObserver interstitial_timer_observer(tab);
    ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
    content::WaitForInterstitialAttach(tab);
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    EXPECT_TRUE(interstitial_timer_observer.timer_started());

    // Check that a MITM software interstitial was not recorded in histogram.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 0);
  }

  // Returns the https server. Guaranteed to be non-NULL.
  const net::EmbeddedTestServer* https_server() const { return &https_server_; }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIMITMSoftwareTest);
};

// The SSLUIMITMSoftwareEnabled and Disabled test classes exist so that the
// scoped feature list can be instantiated in the set up method of the class
// rather than in the test itself. Bug crbug.com/713390 was causing some of the
// tests in SSLUIMITMSoftwareTest to be flaky. Refactoring these tests so that
// the scoped feature list initialization is done in the set up method fixes
// this flakiness.

class SSLUIMITMSoftwareEnabledTest : public SSLUIMITMSoftwareTest {
 public:
  SSLUIMITMSoftwareEnabledTest() {}
  ~SSLUIMITMSoftwareEnabledTest() override {}

  void SetUpOnMainThread() override {
    SSLUIMITMSoftwareTest::SetUpOnMainThread();
    scoped_feature_list_.InitFromCommandLine(
        "MITMSoftwareInterstitial" /* enabled */, std::string() /* disabled */);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIMITMSoftwareEnabledTest);
};

class SSLUIMITMSoftwareDisabledTest : public SSLUIMITMSoftwareTest {
 public:
  SSLUIMITMSoftwareDisabledTest() {}
  ~SSLUIMITMSoftwareDisabledTest() override {}

  void SetUpOnMainThread() override {
    SSLUIMITMSoftwareTest::SetUpOnMainThread();
    scoped_feature_list_.InitFromCommandLine(
        std::string() /* enabled */, "MITMSoftwareInterstitial" /* disabled */);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLUIMITMSoftwareDisabledTest);
};

}  // namespace

// Tests that the MITM software interstitial is not displayed when the feature
// is disabled by Finch.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareDisabledTest, DisabledWithFinch) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is displayed when the feature is
// enabled by Finch.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, EnabledWithFinch) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();
}

// Tests that if a certificates matches the common name of a known MITM software
// cert on the list but not the organization name, the MITM software
// interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(
    SSLUIMITMSoftwareEnabledTest,
    CertificateCommonNameMatchOnly_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexed that
  // the certificate issued by our server won't match.
  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name(kTestMITMSoftwareName);
  mitm_software->set_issuer_common_name_regex(
      https_server()->GetCertificate().get()->issuer().common_name);
  mitm_software->set_issuer_organization_regex(
      "pattern-that-does-not-match-anything");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if a certificates matches the organization name of a known MITM
// software cert on the list but not the common name, the MITM software
// interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(
    SSLUIMITMSoftwareEnabledTest,
    CertificateOrganizationMatchOnly_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexed that
  // the certificate issued by our server won't match.
  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name(kTestMITMSoftwareName);
  mitm_software->set_issuer_common_name_regex(
      "pattern-that-does-not-match-anything");
  mitm_software->set_issuer_organization_regex(
      https_server()->GetCertificate().get()->issuer().organization_names[0]);
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if the certificate does not match any entry on the list of known
// MITM software, the MITM software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       NonMatchingCertificate_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  ASSERT_TRUE(https_server()->Start());

  // Set up an error assistant proto with a list of MITM software regexes that
  // the certificate issued by our server won't match.
  auto config_proto =
      base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  chrome_browser_ssl::MITMSoftware* mitm_software =
      config_proto->add_mitm_software();
  mitm_software->set_name("Non-Matching MITM Software");
  mitm_software->set_issuer_common_name_regex(
      "pattern-that-does-not-match-anything");
  mitm_software->set_issuer_organization_regex(
      "pattern-that-does-not-match-anything");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  ASSERT_TRUE(SSLErrorHandler::GetErrorAssistantProtoVersionIdForTesting() > 0);

  // Navigate to an unsafe page on the server. Mock out the URL host name to
  // equal the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(), GetHSTSTestURL());
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that a MITM software interstitial was not recorded in histogram.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that if there is more than one error on the certificate the MITM
// software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       TwoCertErrors_NoMITMSoftwareInterstitial) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID |
                    net::CERT_STATUS_COMMON_NAME_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that a certificate error other than |CERT_STATUS_AUTHORITY_INVALID|
// will not trigger the MITM software interstitial.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       WrongCertError_NoMITMSoftwareInterstitial) {
  SetUpCertVerifier(net::CERT_STATUS_COMMON_NAME_INVALID);
  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestNoMITMSoftwareInterstitial();
}

// Tests that if the error on the certificate served is overridable the MITM
// software interstitial will not be displayed.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       OverridableError_NoMITMSoftwareInterstitial) {
  base::HistogramTester histograms;

  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);

  ASSERT_TRUE(https_server()->Start());
  SetUpMITMSoftwareCertList(kLargeVersionId);

  // Navigate to an unsafe page to trigger an interstitial, but don't replace
  // the host name with the one set for HSTS.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  SSLInterstitialTimerObserver interstitial_timer_observer(tab);
  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/ssl/blank_page.html"));
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  EXPECT_TRUE(interstitial_timer_observer.timer_started());

  // Check that the histogram for an overridable SSL interstitial was
  // recorded.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

// Tests that the correct strings are displayed on the interstitial in the
// enterprise managed case.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, EnterpriseManaged) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(true);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());

  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();

  InterstitialPage* interstitial_page = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  EXPECT_TRUE(WaitForRenderFrameReady(interstitial_page->GetMainFrame()));

  const std::string expected_primary_paragraph = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_PRIMARY_PARAGRAPH_ENTERPRISE,
      net::EscapeForHTML(base::UTF8ToUTF16(kTestMITMSoftwareName)));
  const std::string expected_explanation = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_EXPLANATION_ENTERPRISE,
      net::EscapeForHTML(base::UTF8ToUTF16(kTestMITMSoftwareName)),
      l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION));

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial_page, expected_primary_paragraph));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial_page, expected_explanation));
}

// Tests that the correct strings are displayed on the interstitial in the
// non-enterprise managed case.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest, NotEnterpriseManaged) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(false);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());

  SetUpMITMSoftwareCertList(kLargeVersionId);
  TestMITMSoftwareInterstitial();

  InterstitialPage* interstitial_page = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  EXPECT_TRUE(WaitForRenderFrameReady(interstitial_page->GetMainFrame()));

  // Don't check the primary paragraph in the non-enterprise case, because it
  // has escaped HTML characters which throw an error.
  const std::string expected_explanation = l10n_util::GetStringFUTF8(
      IDS_MITM_SOFTWARE_EXPLANATION_NONENTERPRISE,
      net::EscapeForHTML(base::UTF8ToUTF16(kTestMITMSoftwareName)),
      l10n_util::GetStringUTF16(IDS_MITM_SOFTWARE_EXPLANATION));

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial_page, expected_explanation));
}

// Initialize MITMSoftware certificate list but set the version_id to zero. This
// less than the version_id of the local resource bundle, so the dynamic
// update will be ignored and a non-MITM interstitial will be shown.
IN_PROC_BROWSER_TEST_F(SSLUIMITMSoftwareEnabledTest,
                       IgnoreDynamicUpdateWithSmallVersionId) {
  SetUpCertVerifier(net::CERT_STATUS_AUTHORITY_INVALID);
  SSLErrorHandler::SetEnterpriseManagedForTesting(false);
  ASSERT_TRUE(SSLErrorHandler::IsEnterpriseManagedFlagSetForTesting());

  SetUpMITMSoftwareCertList(0u);
  TestNoMITMSoftwareInterstitial();
}

class SuperfishSSLUITest : public CertVerifierBrowserTest {
 public:
  SuperfishSSLUITest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SuperfishSSLUITest() override {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  void SetUpCertVerifier(bool use_superfish_cert) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        use_superfish_cert ? CreateSuperfishChain()
                           : net::ImportCertFromFile(
                                 net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(verify_result.verified_cert);

    verify_result.cert_status = net::CERT_STATUS_DATE_INVALID;

    // Collect the hashes of the leaf and intermediates.
    verify_result.public_key_hashes.push_back(
        GetSPKIHash(verify_result.verified_cert.get()));
    for (const net::X509Certificate::OSCertHandle& intermediate :
         verify_result.verified_cert->GetIntermediateCertificates()) {
      scoped_refptr<net::X509Certificate> intermediate_x509 =
          net::X509Certificate::CreateFromHandle(
              intermediate, net::X509Certificate::OSCertHandles());
      verify_result.public_key_hashes.push_back(
          GetSPKIHash(intermediate_x509.get()));
    }

    mock_cert_verifier()->AddResultForCert(https_server_.GetCertificate().get(),
                                           verify_result,
                                           net::ERR_CERT_DATE_INVALID);
  }

  net::EmbeddedTestServer https_server_;

 private:
  static scoped_refptr<net::X509Certificate> CreateSuperfishChain() {
    const std::vector<std::string> certs = {
        // The contents of the leaf certificate don't matter; any cert will do.
        "MIIDczCCAlugAwIBAgIBAjANBgkqhkiG9w0BAQsFADAXMRUwEwYDVQQDDAxUZXN0"
        "IFJvb3QgQ0EwHhcNMTQwODE0MDMwNTI5WhcNMjQwODExMDMwNTI5WjBgMQswCQYD"
        "VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4g"
        "VmlldzEQMA4GA1UECgwHVGVzdCBDQTESMBAGA1UEAwwJMTI3LjAuMC4xMIIBIjAN"
        "BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtfj0Mtj19GXK6dDL3emXoW6Q4vSy"
        "shbQm+KZV+17xltvScGUAKkNXbU19Dp7PBgGo3haaP+mBR99EAiuCWzc7924l55s"
        "zsug3DMrHpXvHfvT2vg+V+2Ljp6GTRKmDDAj7whFTyESQoiHAdilMp+3OO9grbdH"
        "aztLplwrVnJc0bU4h5nsO//GAu+GOO7iBcbwZuIYkVDlVyMnmbvbSSSIZqgUln4a"
        "bSrh/xj1ajfSiKh5yblQ9ZpoCwSeaAIdoXHgiRW6KkgGenjT0Qx3g5iD+LniYCCS"
        "B5vUyMD6WlqdJkDCNWUA86Di0yFNpcSRiJAUp173E7fqK6K914QYGrd7XQIDAQAB"
        "o4GAMH4wDAYDVR0TAQH/BAIwADAdBgNVHQ4EFgQUggQdvQVxg2/2mBlNTxFGiE2b"
        "v6gwHwYDVR0jBBgwFoAUvPcw0TzA8nn675/JbFyT84poq4MwHQYDVR0lBBYwFAYI"
        "KwYBBQUHAwEGCCsGAQUFBwMCMA8GA1UdEQQIMAaHBH8AAAEwDQYJKoZIhvcNAQEL"
        "BQADggEBAITcEASNvT/BPvtoSz815F0C63PmDnQW5MUwawWUTpxpEF56r6R2xiin"
        "GsFcfh1eHF6Hl/5cWyhHMbF5Svg29rFSuNWra4bv7D3tUAtAN2ULIjq3r9QENvDw"
        "0poWaV2LJQP2BYdeSL0lFcQ7au1j2IdVjj4cRN7rG93Ec8emahJtSNXlEmqoVSYm"
        "DX68zXGFsYp25FoaxZwmv9deVxT6tlLPhZAK6H9p4bCUG6xkWuk4zFOe/cbU4V6c"
        "NyIuS9mBX1nhQ6d77acjIP0EkfAdTmzA3quaGStPAKMdWHTJMm7uNbYzTGSNbuyo"
        "jtczxzPGkorOtfZdjhJS7J0Kz0s73fM=",
        // The Superfish root certificate.
        "MIIC9TCCAl6gAwIBAgIJANL8E4epRNznMA0GCSqGSIb3DQEBBQUAMFsxGDAWBgNV"
        "BAoTD1N1cGVyZmlzaCwgSW5jLjELMAkGA1UEBxMCU0YxCzAJBgNVBAgTAkNBMQsw"
        "CQYDVQQGEwJVUzEYMBYGA1UEAxMPU3VwZXJmaXNoLCBJbmMuMB4XDTE0MDUxMjE2"
        "MjUyNloXDTM0MDUwNzE2MjUyNlowWzEYMBYGA1UEChMPU3VwZXJmaXNoLCBJbmMu"
        "MQswCQYDVQQHEwJTRjELMAkGA1UECBMCQ0ExCzAJBgNVBAYTAlVTMRgwFgYDVQQD"
        "Ew9TdXBlcmZpc2gsIEluYy4wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAOjz"
        "Shh2Xxk/sc9Y6X9DBwmVgDXFD/5xMSeBmRImIKXfj2r8QlU57gk4idngNsSsAYJb"
        "1Tnm+Y8HiN/+7vahFM6pdEXY/fAXVyqC4XouEpNarIrXFWPRt5tVgA9YvBxJ7SBi"
        "3bZMpTrrHD2g/3pxptMQeDOuS8Ic/ZJKocPnQaQtAgMBAAGjgcAwgb0wDAYDVR0T"
        "BAUwAwEB/zAdBgNVHQ4EFgQU+5izU38URC7o7tUJml4OVoaoNYgwgY0GA1UdIwSB"
        "hTCBgoAU+5izU38URC7o7tUJml4OVoaoNYihX6RdMFsxGDAWBgNVBAoTD1N1cGVy"
        "ZmlzaCwgSW5jLjELMAkGA1UEBxMCU0YxCzAJBgNVBAgTAkNBMQswCQYDVQQGEwJV"
        "UzEYMBYGA1UEAxMPU3VwZXJmaXNoLCBJbmMuggkA0vwTh6lE3OcwDQYJKoZIhvcN"
        "AQEFBQADgYEApHyg7ApKx3DEcWjzOyLi3JyN0JL+c35yK1VEmxu0Qusfr76645Oj"
        "1IsYwpTws6a9ZTRMzST4GQvFFQra81eLqYbPbMPuhC+FCxkUF5i0DNSWi+kczJXJ"
        "TtCqSwGl9t9JEoFqvtW+znZ9TqyLiOMw7TGEUI+88VAqW0qmXnwPcfo="};

    std::vector<std::string> decoded_certs;
    for (const auto& cert : certs) {
      std::string decoded;
      if (!base::Base64Decode(cert, &decoded))
        return nullptr;
      decoded_certs.push_back(decoded);
    }
    std::vector<base::StringPiece> decoded_pieces;
    for (const auto& decoded : decoded_certs) {
      decoded_pieces.push_back(decoded);
    }
    return net::X509Certificate::CreateFromDERCertChain(decoded_pieces);
  }
};

// Tests that the Superfish histogram is recorded properly when the Superfish
// certificate is present.
IN_PROC_BROWSER_TEST_F(SuperfishSSLUITest, SuperfishRecorded) {
  SetUpCertVerifier(true /* use superfish cert */);
  GURL url(https_server_.GetURL("/ssl/google.html"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), url);
  histograms.ExpectUniqueSample("interstitial.ssl_error_handler.superfish",
                                true, 1);
}

// Tests that the Superfish histogram is recorded properly when the Superfish
// certificate is not present.
IN_PROC_BROWSER_TEST_F(SuperfishSSLUITest, NoSuperfishRecorded) {
  SetUpCertVerifier(false /* use superfish cert */);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  histograms.ExpectUniqueSample("interstitial.ssl_error_handler.superfish",
                                false, 1);
}

// Tests that the Superfish interstitial is shown when the Finch feature is
// enabled and the Superfish certificate is present.
IN_PROC_BROWSER_TEST_F(SuperfishSSLUITest, SuperfishInterstitial) {
  base::HistogramTester histograms;
  const char kDecisionHistogram[] = "interstitial.superfish.decision";
  const char kInteractionHistogram[] = "interstitial.superfish.interaction";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("SuperfishInterstitial",
                                          std::string());
  SetUpCertVerifier(true /* use superfish cert */);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  EXPECT_TRUE(WaitForRenderFrameReady(interstitial_page->GetMainFrame()));
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());

  // Look for keywords on the page to check that the Superfish interstitial is
  // showing.
  const std::string expected_title =
      l10n_util::GetStringUTF8(IDS_SSL_SUPERFISH_HEADING);
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial_page, expected_title));

  // Check that the correct histograms were recorded.
  histograms.ExpectTotalCount(kDecisionHistogram, 1);
  histograms.ExpectBucketCount(kDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectTotalCount(kInteractionHistogram, 1);
  histograms.ExpectBucketCount(
      kInteractionHistogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

// Tests that the Superfish interstitial is not shown when the Finch feature is
// disabled.
IN_PROC_BROWSER_TEST_F(SuperfishSSLUITest, SuperfishInterstitialDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(std::string(),
                                          "SuperfishInterstitial");
  SetUpCertVerifier(true /* use superfish cert */);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForInterstitialAttach(tab);
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  EXPECT_TRUE(WaitForRenderFrameReady(interstitial_page->GetMainFrame()));
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());

  // Look for keywords on the page to check that the Superfish interstitial is
  // not showing.
  const std::string expected_title =
      l10n_util::GetStringUTF8(IDS_SSL_V2_HEADING);
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial_page, expected_title));
}

// Allows tests to effectively turn off CT requirements. Used by
// SymantecMessageSSLUITest below to test Symantec certificates issued after the
// CT requirement date.
class NoRequireCTDelegate
    : public net::TransportSecurityState::RequireCTDelegate {
 public:
  NoRequireCTDelegate() {}
  ~NoRequireCTDelegate() override = default;

  CTRequirementLevel IsCTRequiredForHost(const std::string& hostname) override {
    return CTRequirementLevel::NOT_REQUIRED;
  }
};

void SetRequireCTDelegateOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    net::TransportSecurityState::RequireCTDelegate* delegate) {
  net::TransportSecurityState* state =
      context_getter->GetURLRequestContext()->transport_security_state();
  state->SetRequireCTDelegate(delegate);
}

// A test fixture that mocks certificate verifications for legacy Symantec
// certificates that are slated to be distrusted in future Chrome releases.
class SymantecMessageSSLUITest : public CertVerifierBrowserTest {
 public:
  SymantecMessageSSLUITest()
      : CertVerifierBrowserTest(),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SymantecMessageSSLUITest() override {}

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));

    require_ct_delegate_ = base::MakeUnique<NoRequireCTDelegate>();
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &SetRequireCTDelegateOnIOThread,
            base::RetainedRef(browser()->profile()->GetRequestContext()),
            require_ct_delegate_.get()));
  }

 protected:
  void SetUpCertVerifier(bool use_chrome_66_date) {
    net::CertVerifyResult verify_result;
    {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      verify_result.verified_cert = net::CreateCertificateChainFromFile(
          net::GetTestCertsDirectory(),
          use_chrome_66_date ? "pre_june_2016.pem" : "post_june_2016.pem",
          net::X509Certificate::FORMAT_AUTO);
    }
    ASSERT_TRUE(verify_result.verified_cert);
    verify_result.cert_status = 0;

    // Collect the hashes of the leaf and intermediates.
    verify_result.public_key_hashes.push_back(
        GetSPKIHash(verify_result.verified_cert.get()));
    for (const net::X509Certificate::OSCertHandle& intermediate :
         verify_result.verified_cert->GetIntermediateCertificates()) {
      scoped_refptr<net::X509Certificate> intermediate_x509 =
          net::X509Certificate::CreateFromHandle(
              intermediate, net::X509Certificate::OSCertHandles());
      ASSERT_TRUE(intermediate_x509);
      verify_result.public_key_hashes.push_back(
          GetSPKIHash(intermediate_x509.get()));
    }

    mock_cert_verifier()->AddResultForCert(https_server_.GetCertificate().get(),
                                           verify_result, net::OK);
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<NoRequireCTDelegate> require_ct_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SymantecMessageSSLUITest);
};

// Tests that the Symantec console message is properly overridden for pre-June
// 2016 certificates.
IN_PROC_BROWSER_TEST_F(SymantecMessageSSLUITest, PreJune2016) {
  ASSERT_NO_FATAL_FAILURE(
      SetUpCertVerifier(true /* use Chrome 66 distrust date */));
  ASSERT_TRUE(https_server()->Start());
  GURL url(https_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(
      tab, "*The SSL certificate used to load*ssl/google.html*");
  tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), url);
  console_observer.Wait();
  EXPECT_TRUE(
      base::MatchPattern(console_observer.message(), "*distrusted in M66*"));
}

// Tests that the Symantec console message is properly overridden for post-June
// 2016 certificates.
IN_PROC_BROWSER_TEST_F(SymantecMessageSSLUITest, PostJune2016) {
  ASSERT_NO_FATAL_FAILURE(
      SetUpCertVerifier(false /* use Chrome 66 distrust date */));
  ASSERT_TRUE(https_server()->Start());
  GURL url(https_server()->GetURL("/ssl/google.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(
      tab, "*The SSL certificate used to load*/ssl/google.html*");
  tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), url);
  console_observer.Wait();
  EXPECT_TRUE(
      base::MatchPattern(console_observer.message(), "*distrusted in M70*"));
}

// Tests that the Symantec console message is logged for subresources.
IN_PROC_BROWSER_TEST_F(SymantecMessageSSLUITest, Subresource) {
  ASSERT_NO_FATAL_FAILURE(
      SetUpCertVerifier(false /* use Chrome 66 distrust date */));
  ASSERT_TRUE(https_server()->Start());
  GURL url(https_server()->GetURL("/ssl/page_with_subresource.html"));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::ConsoleObserverDelegate console_observer(tab,
                                                    "*google_files/logo.gif*");
  tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), url);
  console_observer.Wait();
  EXPECT_TRUE(base::MatchPattern(console_observer.message(),
                                 "*The SSL certificate used to load*"));
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest insecure content in synchronous mode.

// XMLHttpRequest insecure content in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.
