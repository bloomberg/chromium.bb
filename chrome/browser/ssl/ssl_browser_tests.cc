// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/bad_clock_blocking_page.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_state/security_state_model.h"
#include "components/security_state/switches.h"
#include "components/ssl_errors/error_classification.h"
#include "components/variations/variations_associated_data.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/security_style.h"
#include "content/public/common/ssl_status.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/url_request/url_request_context.h"

#if defined(USE_NSS_CERTS)
#include "chrome/browser/net/nss_context.h"
#include "net/base/crypto_module.h"
#include "net/cert/nss_cert_database.h"
#endif  // defined(USE_NSS_CERTS)

using base::ASCIIToUTF16;
using chrome_browser_interstitials::SecurityInterstitialIDNTest;
using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::SSLStatus;
using content::WebContents;
using web_modal::WebContentsModalDialogManager;

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

namespace {

enum ProceedDecision {
  SSL_INTERSTITIAL_PROCEED,
  SSL_INTERSTITIAL_DO_NOT_PROCEED
};

class ProvisionalLoadWaiter : public content::WebContentsObserver {
 public:
  explicit ProvisionalLoadWaiter(WebContents* tab)
    : WebContentsObserver(tab), waiting_(false), seen_(false) {}

  void Wait() {
    if (seen_)
      return;

    waiting_ = true;
    content::RunMessageLoop();
  }

  void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override {
    seen_ = true;
    if (waiting_)
      base::MessageLoopForUI::current()->QuitWhenIdle();
  }

 private:
  bool waiting_;
  bool seen_;
};

namespace AuthState {

enum AuthStateFlags {
  NONE = 0,
  DISPLAYED_INSECURE_CONTENT = 1 << 0,
  RAN_INSECURE_CONTENT = 1 << 1,
  SHOWING_INTERSTITIAL = 1 << 2,
  SHOWING_ERROR = 1 << 3
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
}

}  // namespace AuthState

namespace SecurityStyle {

void Check(const NavigationEntry& entry,
           content::SecurityStyle expected_security_style) {
  EXPECT_EQ(expected_security_style, entry.GetSSL().security_style);
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
    if (extra_cert_errors)
      LOG(WARNING) << "Got unexpected cert error: " << extra_cert_errors;
  } else {
    EXPECT_EQ(0U, entry.GetSSL().cert_status & net::CERT_STATUS_ALL_ERRORS);
  }
}

}  // namespace CertError

void CheckSecurityState(WebContents* tab,
                        net::CertStatus error,
                        content::SecurityStyle expected_security_style,
                        int expected_authentication_state) {
  ASSERT_FALSE(tab->IsCrashed());
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  CertError::Check(*entry, error);
  SecurityStyle::Check(*entry, expected_security_style);
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
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTest(&callback_);
  }

  ~SSLInterstitialTimerObserver() {
    SSLErrorHandler::SetInterstitialTimerStartedCallbackForTest(nullptr);
  }

  // Waits until the interstitial delay timer in SSLErrorHandler is started.
  void WaitForTimerStarted() { message_loop_runner_->Run(); }

 private:
  void OnTimerStarted(content::WebContents* web_contents) {
    if (web_contents_ == web_contents)
      message_loop_runner_->Quit();
  }

  const content::WebContents* web_contents_;
  SSLErrorHandler::TimerStartedCallback callback_;

  std::unique_ptr<base::RunLoop> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLInterstitialTimerObserver);
};

// Checks that two SSLStatuses will result in the same security UI: that
// is, the cert ids can differ as long as they refer to the same cert,
// and otherwise SSLStatus::Equals() must be true.
void CheckSSLStatusesEquals(const content::SSLStatus& one,
                            const content::SSLStatus& two) {
  content::CertStore* cert_store = content::CertStore::GetInstance();
  scoped_refptr<net::X509Certificate> cert1;
  scoped_refptr<net::X509Certificate> cert2;
  cert_store->RetrieveCert(one.cert_id, &cert1);
  cert_store->RetrieveCert(two.cert_id, &cert2);
  EXPECT_TRUE(cert1 && cert2);
  EXPECT_TRUE(cert1->Equals(cert2.get()));

  SSLStatus one_without_cert_id = one;
  one_without_cert_id.cert_id = 0;
  SSLStatus two_without_cert_id = two;
  two_without_cert_id.cert_id = 0;
  EXPECT_TRUE(one_without_cert_id.Equals(two_without_cert_id));
}

}  // namespace

class SSLUITest
    : public certificate_reporting_test_utils::CertificateReportingTest {
 public:
  SSLUITest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_expired_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS),
        wss_server_expired_(net::SpawnedTestServer::TYPE_WSS,
                            SSLOptions(SSLOptions::CERT_EXPIRED),
                            net::GetWebSocketTestDataDirectory()) {
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));

    https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    https_server_expired_.AddDefaultHandlers(base::FilePath(kDocRoot));

    https_server_mismatched_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_.AddDefaultHandlers(base::FilePath(kDocRoot));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    // Use process-per-site so that navigating to a same-site page in a
    // new tab will use the same process.
    command_line->AppendSwitch(switches::kProcessPerSite);
  }

  void CheckAuthenticatedState(WebContents* tab,
                               int expected_authentication_state) {
    CheckSecurityState(tab,
                       CertError::NONE,
                       content::SECURITY_STYLE_AUTHENTICATED,
                       expected_authentication_state);
  }

  void CheckUnauthenticatedState(WebContents* tab,
                                 int expected_authentication_state) {
    CheckSecurityState(tab,
                       CertError::NONE,
                       content::SECURITY_STYLE_UNAUTHENTICATED,
                       expected_authentication_state);
  }

  void CheckAuthenticationBrokenState(WebContents* tab,
                                      net::CertStatus error,
                                      int expected_authentication_state) {
    CheckSecurityState(tab,
                       error,
                       content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
                       expected_authentication_state);
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION doesn't lower the security style
    // to SECURITY_STYLE_AUTHENTICATION_BROKEN.
    ASSERT_NE(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION, error);
  }

  void CheckWorkerLoadResult(WebContents* tab, bool expected_load) {
    // Workers are async and we don't have notifications for them passing
    // messages since they do it between renderer and worker processes.
    // So have a polling loop, check every 200ms, timeout at 30s.
    const int kTimeoutMS = 200;
    base::Time time_to_quit = base::Time::Now() +
        base::TimeDelta::FromMilliseconds(30000);

    while (base::Time::Now() < time_to_quit) {
      bool worker_finished = false;
      ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
          tab,
          "window.domAutomationController.send(IsWorkerFinished());",
          &worker_finished));

      if (worker_finished)
        break;

      // Wait a bit.
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
          base::TimeDelta::FromMilliseconds(kTimeoutMS));
      content::RunMessageLoop();
    }

    bool actually_loaded_content = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(IsContentLoaded());",
        &actually_loaded_content));
    EXPECT_EQ(expected_load, actually_loaded_content);
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

  bool IsShowingWebContentsModalDialog() const {
    return WebContentsModalDialogManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())->
            IsDialogActive();
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
        make_pair("REPLACE_WITH_HTTP_PAGE", http_url.spec()));
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

  static void GetPageWithUnsafeWorkerPath(
      const net::EmbeddedTestServer& https_server,
      std::string* page_with_unsafe_worker_path) {
    // Get the "imported.js" URL from the expired https server and
    // substitute it into the unsafe_worker.js file.
    GURL imported_js_url = https_server.GetURL("/ssl/imported.js");
    base::StringPairs replacement_text_for_unsafe_worker;
    replacement_text_for_unsafe_worker.push_back(
        make_pair("REPLACE_WITH_IMPORTED_JS_URL", imported_js_url.spec()));
    std::string unsafe_worker_path;
    net::test_server::GetFilePathWithReplacements(
        "unsafe_worker.js", replacement_text_for_unsafe_worker,
        &unsafe_worker_path);

    // Now, substitute this into the page with unsafe worker.
    base::StringPairs replacement_text_for_page_with_unsafe_worker;
    replacement_text_for_page_with_unsafe_worker.push_back(
        make_pair("REPLACE_WITH_UNSAFE_WORKER_PATH", unsafe_worker_path));
    net::test_server::GetFilePathWithReplacements(
        "/ssl/page_with_unsafe_worker.html",
        replacement_text_for_page_with_unsafe_worker,
        page_with_unsafe_worker_path);
  }

  // Helper function for testing invalid certificate chain reporting.
  void TestBrokenHTTPSReporting(
      certificate_reporting_test_utils::OptIn opt_in,
      ProceedDecision proceed,
      certificate_reporting_test_utils::ExpectReport expect_report,
      Browser* browser) {
    base::RunLoop run_loop;
    ASSERT_TRUE(https_server_expired_.Start());

    ASSERT_NO_FATAL_FAILURE(SetUpMockReporter());

    // Opt in to sending reports for invalid certificate chains.
    certificate_reporting_test_utils::SetCertReportingOptIn(browser, opt_in);

    ui_test_utils::NavigateToURL(browser, https_server_expired_.GetURL("/"));

    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(tab != nullptr);
    CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                   AuthState::SHOWING_INTERSTITIAL);

    std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
        certificate_reporting_test_utils::SetUpMockSSLCertReporter(
            &run_loop, expect_report);

    ASSERT_TRUE(tab->GetInterstitialPage() != nullptr);
    SSLBlockingPage* interstitial_page = static_cast<SSLBlockingPage*>(
        tab->GetInterstitialPage()->GetDelegateForTesting());
    ASSERT_TRUE(interstitial_page != nullptr);
    interstitial_page->SetSSLCertReporterForTesting(
        std::move(ssl_cert_reporter));

    EXPECT_EQ(std::string(), GetLatestHostnameReported());

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
      EXPECT_EQ(https_server_expired_.GetURL("/").host(),
                GetLatestHostnameReported());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(std::string(), GetLatestHostnameReported());
    }
  }

  // Helper function for testing invalid certificate chain reporting with the
  // bad clock interstitial.
  void TestBadClockReporting(
      certificate_reporting_test_utils::OptIn opt_in,
      certificate_reporting_test_utils::ExpectReport expect_report,
      Browser* browser) {
    base::RunLoop run_loop;
    ASSERT_TRUE(https_server_expired_.Start());
    ASSERT_NO_FATAL_FAILURE(SetUpMockReporter());

    // Set network time back ten minutes, which is sufficient to
    // trigger the reporting.
    g_browser_process->network_time_tracker()->UpdateNetworkTime(
        base::Time::Now() - base::TimeDelta::FromMinutes(10),
        base::TimeDelta::FromMilliseconds(1),   /* resolution */
        base::TimeDelta::FromMilliseconds(500), /* latency */
        base::TimeTicks::Now() /* posting time of this update */);

    // Opt in to sending reports for invalid certificate chains.
    certificate_reporting_test_utils::SetCertReportingOptIn(browser, opt_in);

    ui_test_utils::NavigateToURL(browser, https_server_expired_.GetURL("/"));

    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    CheckAuthenticationBrokenState(tab, net::CERT_STATUS_DATE_INVALID,
                                   AuthState::SHOWING_INTERSTITIAL);

    std::unique_ptr<SSLCertReporter> ssl_cert_reporter =
        certificate_reporting_test_utils::SetUpMockSSLCertReporter(
            &run_loop, expect_report);

    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_EQ(BadClockBlockingPage::kTypeForTesting,
              interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
    BadClockBlockingPage* clock_page = static_cast<BadClockBlockingPage*>(
        tab->GetInterstitialPage()->GetDelegateForTesting());
    clock_page->SetSSLCertReporterForTesting(std::move(ssl_cert_reporter));

    EXPECT_EQ(std::string(), GetLatestHostnameReported());

    interstitial_page->DontProceed();

    if (expect_report ==
        certificate_reporting_test_utils::CERT_REPORT_EXPECTED) {
      // Check that the mock reporter received a request to send a report.
      run_loop.Run();
      EXPECT_EQ(https_server_expired_.GetURL("/").host(),
                GetLatestHostnameReported());
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(std::string(), GetLatestHostnameReported());
    }
  }

  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_server_expired_;
  net::EmbeddedTestServer https_server_mismatched_;
  net::SpawnedTestServer wss_server_expired_;

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

  DISALLOW_COPY_AND_ASSIGN(SSLUITest);
};

class SSLUITestBlock : public SSLUITest {
 public:
  SSLUITestBlock() : SSLUITest() {}

  // Browser will neither run nor display insecure content.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kNoDisplayingInsecureContent);
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
  SSLUITestWithExtendedReporting() : SSLUITest() {}
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

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBrokenHTTPSMetricsReporting_Proceed) {
  ASSERT_TRUE(https_server_expired_.Start());
  ASSERT_NO_FATAL_FAILURE(SetUpMockReporter());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
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
  ASSERT_NO_FATAL_FAILURE(SetUpMockReporter());
  base::HistogramTester histograms;
  const std::string decision_histogram =
      "interstitial.ssl_overridable.decision";
  const std::string interaction_histogram =
      "interstitial.ssl_overridable.interaction";

  // Histograms should start off empty.
  histograms.ExpectTotalCount(decision_histogram, 0);
  histograms.ExpectTotalCount(interaction_histogram, 0);

  // After navigating to the page, the totals should be set.
  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
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

// http://crbug.com/91745
#if defined(OS_CHROMEOS)
#define MAYBE_TestOKHTTPS DISABLED_TestOKHTTPS
#else
#define MAYBE_TestOKHTTPS TestOKHTTPS
#endif

// Visits a page over OK https:
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestOKHTTPS) {
  ASSERT_TRUE(https_server_.Start());

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Visits a page with https error and proceed:
#if defined(OS_LINUX)
// flaky http://crbug.com/396462
#define MAYBE_TestHTTPSExpiredCertAndProceed \
    DISABLED_TestHTTPSExpiredCertAndProceed
#else
#define MAYBE_TestHTTPSExpiredCertAndProceed TestHTTPSExpiredCertAndProceed
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSExpiredCertAndProceed) {
  ASSERT_TRUE(https_server_expired_.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server_expired_.GetURL("/ssl/google.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::SHOWING_INTERSTITIAL);

  ProceedThroughInterstitial(tab);

  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

#ifndef NEDBUG
// Flaky on Windows debug (http://crbug.com/280537).
#define MAYBE_TestHTTPSExpiredCertAndDontProceed \
        DISABLED_TestHTTPSExpiredCertAndDontProceed
#else
#define MAYBE_TestHTTPSExpiredCertAndDontProceed \
        TestHTTPSExpiredCertAndDontProceed
#endif

// Visits a page with https error and don't proceed (and ensure we can still
// navigate at that point):
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSExpiredCertAndDontProceed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // First navigate to an OK page.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL("/ssl/google.html"));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  NavigationEntry* entry = tab->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);

  GURL cross_site_url = https_server_expired_.GetURL("/ssl/google.html");
  // Change the host name from 127.0.0.1 to localhost so it triggers a
  // cross-site navigation so we can test http://crbug.com/5800 is gone.
  ASSERT_EQ("127.0.0.1", cross_site_url.host());
  GURL::Replacements replacements;
  replacements.SetHostStr("localhost");
  cross_site_url = cross_site_url.ReplaceComponents(replacements);

  // Now go to a bad HTTPS page.
  ui_test_utils::NavigateToURL(browser(), cross_site_url);

  // An interstitial should be showing.
  CheckAuthenticationBrokenState(tab,
                                 net::CERT_STATUS_COMMON_NAME_INVALID,
                                 AuthState::SHOWING_INTERSTITIAL);

  // Simulate user clicking "Take me back".
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  interstitial_page->DontProceed();

  // We should be back to the original good page.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Try to navigate to a new page. (to make sure bug 5800 is fixed).
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
  SSLErrorHandler::SetClockForTest(mock_clock.get());
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForInterstitialAttach(clock_tab);
  InterstitialPage* clock_interstitial = clock_tab->GetInterstitialPage();
  ASSERT_TRUE(clock_interstitial);
  EXPECT_EQ(BadClockBlockingPage::kTypeForTesting,
            clock_interstitial->GetDelegateForTesting()->GetTypeForTesting());
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

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  WebContents* clock_tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForInterstitialAttach(clock_tab);
  InterstitialPage* clock_interstitial = clock_tab->GetInterstitialPage();
  ASSERT_TRUE(clock_interstitial);
  EXPECT_EQ(BadClockBlockingPage::kTypeForTesting,
            clock_interstitial->GetDelegateForTesting()->GetTypeForTesting());
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

  ProvisionalLoadWaiter load_failed_observer(tab);

  // Simulate user clicking on back button (crbug.com/39248).
  chrome::GoBack(browser(), CURRENT_TAB);

  // Wait until we hear the load failure, and make sure we haven't changed
  // the previous RFH.  Prevents regression of http://crbug.com/82667.
  load_failed_observer.Wait();
  EXPECT_EQ(rfh, tab->GetMainFrame());

  // We should be back at the original good page.
  EXPECT_FALSE(browser()->tab_strip_model()->GetActiveWebContents()->
                   GetInterstitialPage());
  CheckUnauthenticatedState(tab, AuthState::NONE);
}

// Visits a page with https error and then goes back using GoToOffset.
// Disabled because its flaky: http://crbug.com/40932, http://crbug.com/43575.
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

// Visit a HTTP page which request WSS connection to a server providing invalid
// certificate. Close the page while WSS connection waits for SSLManager's
// response from UI thread.
// Disabled on Windows because it was flaking on XP Tests (1). crbug.com/165258
#if defined(OS_WIN)
#define MAYBE_TestWSSInvalidCertAndClose DISABLED_TestWSSInvalidCertAndClose
#else
#define MAYBE_TestWSSInvalidCertAndClose TestWSSInvalidCertAndClose
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestWSSInvalidCertAndClose) {
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

// Ensure that non-standard origins are marked correctly when the
// MarkNonSecureAs field trial is enabled.
IN_PROC_BROWSER_TEST_F(SSLUITest, MarkFileAsNonSecure) {
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(
          "MarkNonSecureAs",
          security_state::switches::kMarkNonSecureAsNonSecure);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);

  ui_test_utils::NavigateToURL(browser(), GURL("file:///"));
  EXPECT_EQ(security_state::SecurityStateModel::NONE,
            model_client->GetSecurityInfo().security_level);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, MarkAboutAsNonSecure) {
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(
          "MarkNonSecureAs",
          security_state::switches::kMarkNonSecureAsNonSecure);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_EQ(security_state::SecurityStateModel::NONE,
            model_client->GetSecurityInfo().security_level);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, MarkDataAsNonSecure) {
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(
          "MarkNonSecureAs",
          security_state::switches::kMarkNonSecureAsNonSecure);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/plain,hello"));
  EXPECT_EQ(security_state::SecurityStateModel::NONE,
            model_client->GetSecurityInfo().security_level);
}

IN_PROC_BROWSER_TEST_F(SSLUITest, MarkBlobAsNonSecure) {
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(
          "MarkNonSecureAs",
          security_state::switches::kMarkNonSecureAsNonSecure);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(contents);
  ASSERT_TRUE(model_client);

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("blob:chrome://newtab/49a463bb-fac8-476c-97bf-5d7076c3ea1a"));
  EXPECT_EQ(security_state::SecurityStateModel::NONE,
            model_client->GetSecurityInfo().security_level);
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
  scoped_refptr<net::CryptoModule> crypt_module = cert_db_->GetPublicModule();
  std::string pkcs12_data;
  base::FilePath cert_path = net::GetTestCertsDirectory().Append(
      FILE_PATH_LITERAL("websocket_client_cert.p12"));
  EXPECT_TRUE(base::ReadFileToString(cert_path, &pkcs12_data));
  EXPECT_EQ(net::OK,
            cert_db_->ImportFromPKCS12(
                crypt_module.get(), pkcs12_data, base::string16(), true, NULL));

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
          std::string(), dict.release());

  // Visit a HTTPS page which requires client certs.
  ui_test_utils::NavigateToURL(browser(), url);
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Test page runs a WebSocket wss connection test. The result will be shown
  // as page title.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(base::LowerCaseEqualsASCII(result, "pass"));
}
#endif  // defined(USE_NSS_CERTS)

// Flaky on CrOS http://crbug.com/92292
#if defined(OS_CHROMEOS)
#define MAYBE_TestHTTPSErrorWithNoNavEntry \
    DISABLED_TestHTTPSErrorWithNoNavEntry
#else
#define MAYBE_TestHTTPSErrorWithNoNavEntry TestHTTPSErrorWithNoNavEntry
#endif  // defined(OS_CHROMEOS)

// Open a page with a HTTPS error in a tab with no prior navigation (through a
// link with a blank target).  This is to test that the lack of navigation entry
// does not cause any problems (it was causing a crasher, see
// http://crbug.com/19941).
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestHTTPSErrorWithNoNavEntry) {
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url = https_server_expired_.GetURL("/ssl/google.htm");
  WebContents* tab2 = chrome::AddSelectedTabWithURL(
      browser(), url, ui::PAGE_TRANSITION_TYPED);
  content::WaitForLoadStop(tab2);

  // Verify our assumption that there was no prior navigation.
  EXPECT_FALSE(chrome::CanGoBack(browser()));

  // We should have an interstitial page showing.
  ASSERT_TRUE(tab2->GetInterstitialPage());
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting, tab2->GetInterstitialPage()
                                                  ->GetDelegateForTesting()
                                                  ->GetTypeForTesting());
}

IN_PROC_BROWSER_TEST_F(SSLUITest, TestBadHTTPSDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());
  GURL url_non_dangerous = embedded_test_server()->GetURL("/");
  GURL url_dangerous =
      https_server_expired_.GetURL("/downloads/dangerous/dangerous.exe");
  base::ScopedTempDir downloads_directory_;

  // Need empty temp dir to avoid having Chrome ask us for a new filename
  // when we've downloaded dangerous.exe one hundred times.
  ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory,
      downloads_directory_.path());

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
  chrome::GoBack(browser(), CURRENT_TAB);

  dangerous_download_observer.WaitForFinished();
}

//
// Insecure content
//

#if defined(OS_WIN)
// http://crbug.com/152940 Flaky on win.
#define MAYBE_TestDisplaysInsecureContent DISABLED_TestDisplaysInsecureContent
#else
#define MAYBE_TestDisplaysInsecureContent TestDisplaysInsecureContent
#endif

// Visits a page that displays insecure content.
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestDisplaysInsecureContent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  // Load a page that displays insecure content.
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::DISPLAYED_INSECURE_CONTENT);
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

// Visits a page with unsafe content and make sure that:
// - frames content is replaced with warning
// - images and scripts are filtered out entirely
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContents) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement("/ssl/page_with_unsafe_contents.html",
                                        https_server_expired_.host_port_pair(),
                                        &replacement_path);
  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  // When the bad content is filtered, the state is expected to be
  // authenticated.
  CheckAuthenticatedState(tab, AuthState::NONE);

  // Because of cross-frame scripting restrictions, we cannot access the iframe
  // content.  So to know if the frame was loaded, we just check if a popup was
  // opened (the iframe content opens one).
  // Note: because of bug 1115868, no web contents modal dialog is opened right
  //       now.  Once the bug is fixed, this will do the real check.
  EXPECT_FALSE(IsShowingWebContentsModalDialog());

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab,
      "window.domAutomationController.send(ImageWidth());",
      &img_width));
  // In order to check that the image was not loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, we assume the broken
  // image is less than 100.
  EXPECT_LT(img_width, 100);

  bool js_result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      tab,
      "window.domAutomationController.send(IsFooSet());",
      &js_result));
  EXPECT_FALSE(js_result);
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

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_with_dynamic_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);
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
  CheckAuthenticatedState(tab, AuthState::DISPLAYED_INSECURE_CONTENT);
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
  params.disposition = NEW_FOREGROUND_TAB;
  params.tabstrip_index = 0;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Navigate(&params);
  WebContents* tab2 = params.target_contents;
  observer.Wait();

  // The new tab has insecure content.
  CheckAuthenticatedState(tab2, AuthState::DISPLAYED_INSECURE_CONTENT);

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
  params.disposition = NEW_FOREGROUND_TAB;
  params.source_contents = tab1;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Navigate(&params);
  WebContents* tab2 = params.target_contents;
  observer.Wait();

  // Both tabs should have the same process.
  EXPECT_EQ(tab1->GetRenderProcessHost(), tab2->GetRenderProcessHost());

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
  CheckAuthenticatedState(tab, AuthState::DISPLAYED_INSECURE_CONTENT);
}

// http://crbug.com/84729
#if defined(OS_CHROMEOS)
#define MAYBE_TestRunsCachedInsecureContent \
    DISABLED_TestRunsCachedInsecureContent
#else
#define MAYBE_TestRunsCachedInsecureContent TestRunsCachedInsecureContent
#endif  // defined(OS_CHROMEOS)

// Visits a page with script over http.  Visits another page over https
// referencing that same script over http (hoping it is coming from the webcore
// memory cache).
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestRunsCachedInsecureContent) {
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

#if defined(OS_CHROMEOS)
// This test seems to be flaky and hang on chromiumos.
// http://crbug.com/84419
#define MAYBE_TestRefNavigation DISABLED_TestRefNavigation
#else
#define MAYBE_TestRefNavigation TestRefNavigation
#endif

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

// Tests that closing a page that has a unsafe pop-up does not crash the
// browser (bug #1966).
// TODO(jcampan): http://crbug.com/2136 disabled because the popup is not
//                opened as it is not initiated by a user gesture.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestCloseTabWithUnsafePopup) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement("/ssl/page_with_unsafe_popup.html",
                                        https_server_expired_.host_port_pair(),
                                        &replacement_path);

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(replacement_path));

  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();
  // It is probably overkill to add a notification for a popup-opening, let's
  // just poll.
  for (int i = 0; i < 10; i++) {
    if (IsShowingWebContentsModalDialog())
      break;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
        base::TimeDelta::FromSeconds(1));
    content::RunMessageLoop();
  }
  ASSERT_TRUE(IsShowingWebContentsModalDialog());

  // Let's add another tab to make sure the browser does not exit when we close
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

// Flaky on Linux. http://crbug.com/368280.
#if defined(OS_LINUX)
#define MAYBE_TestRedirectGoodToBadHTTPS DISABLED_TestRedirectGoodToBadHTTPS
#else
#define MAYBE_TestRedirectGoodToBadHTTPS TestRedirectGoodToBadHTTPS
#endif

// Visit a page over good https that is a redirect to a page with bad https.
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestRedirectGoodToBadHTTPS) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  GURL url1 = https_server_.GetURL("/server-redirect?");
  GURL url2 = https_server_expired_.GetURL("/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(), GURL(url1.spec() + url2.spec()));

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
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

// Flaky on Linux. http://crbug.com/368280.
#if defined(OS_LINUX)
#define MAYBE_TestRedirectHTTPToBadHTTPS DISABLED_TestRedirectHTTPToBadHTTPS
#else
#define MAYBE_TestRedirectHTTPToBadHTTPS TestRedirectHTTPToBadHTTPS
#endif

// Visit a page over http that is a redirect to a page with bad HTTPS.
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestRedirectHTTPToBadHTTPS) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_expired_.Start());

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL http_url = embedded_test_server()->GetURL("/server-redirect?");
  GURL bad_https_url = https_server_expired_.GetURL("/ssl/google.html");
  ui_test_utils::NavigateToURL(browser(),
                               GURL(http_url.spec() + bad_https_url.spec()));
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

  // Our state should be unathenticated (in the ran mixed script sense)
  CheckAuthenticationBrokenState(
      tab,
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);

  // Go back, our state should be unchanged.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    tab->GetController().GoBack();
    observer.Wait();
  }

  CheckAuthenticationBrokenState(
      tab,
      CertError::NONE,
      AuthState::DISPLAYED_INSECURE_CONTENT | AuthState::RAN_INSECURE_CONTENT);
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
  CheckAuthenticationBrokenState(
      tab, net::CERT_STATUS_DATE_INVALID, AuthState::NONE);
}

// From an HTTP top frame, navigate to good and bad HTTPS (security state should
// stay unauthenticated).
// Disabled, flakily exceeds test timeout, http://crbug.com/43437.
IN_PROC_BROWSER_TEST_F(SSLUITest, DISABLED_TestUnauthenticatedFrameNavigation) {
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

IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsInWorkerFiltered) {
  ASSERT_TRUE(https_server_.Start());
  ASSERT_TRUE(https_server_expired_.Start());

  // This page will spawn a Worker which will try to load content from
  // BadCertServer.
  std::string page_with_unsafe_worker_path;
  GetPageWithUnsafeWorkerPath(https_server_expired_,
                              &page_with_unsafe_worker_path);
  ui_test_utils::NavigateToURL(browser(), https_server_.GetURL(
      page_with_unsafe_worker_path));
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  // Expect Worker not to load insecure content.
  CheckWorkerLoadResult(tab, false);
  // The bad content is filtered, expect the state to be authenticated.
  CheckAuthenticatedState(tab, AuthState::NONE);
}

// This test, and the related test TestUnsafeContentsWithUserException, verify
// that if unsafe content is loaded but the host of that unsafe content has a
// user exception, the content runs and the security style is downgraded.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsInWorkerWithUserException) {
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

  // Navigate to safe page that has Worker loading unsafe content.
  // Expect content to load but be marked as auth broken due to running insecure
  // content.
  std::string page_with_unsafe_worker_path;
  GetPageWithUnsafeWorkerPath(https_server_mismatched_,
                              &page_with_unsafe_worker_path);
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL(page_with_unsafe_worker_path));
  CheckWorkerLoadResult(tab, true);  // Worker loads insecure content
  CheckAuthenticationBrokenState(tab, CertError::NONE,
                                 AuthState::RAN_INSECURE_CONTENT);
}

// Visits a page with unsafe content and makes sure that if a user exception to
// the certificate error is present, the image is loaded and script executes.
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeContentsWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(SetUpUnsafeContentsWithUserException(
      "/ssl/page_with_unsafe_contents.html"));
  CheckAuthenticationBrokenState(
      tab, CertError::NONE,
      AuthState::RAN_INSECURE_CONTENT | AuthState::DISPLAYED_INSECURE_CONTENT);

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
  // the main resources don't cause mixed content UI downgrades. (Such
  // errors would be confusing and duplicative.)
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
}

// Like the test above, but only displaying inactive content (an image).
IN_PROC_BROWSER_TEST_F(SSLUITest, TestUnsafeImageWithUserException) {
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(
      SetUpUnsafeContentsWithUserException("/ssl/page_with_unsafe_image.html"));
  CheckAuthenticatedState(tab, AuthState::DISPLAYED_INSECURE_CONTENT);

  int img_width;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      tab, "window.domAutomationController.send(ImageWidth());", &img_width));
  // In order to check that the image was loaded, we check its width.
  // The actual image (Google logo) is 114 pixels wide, so we assume a good
  // image is greater than 100.
  EXPECT_GT(img_width, 100);
}

// Test that when the browser blocks displaying insecure content (images), the
// indicator shows a secure page, because the blocking made the otherwise
// unsafe page safe (the notification of this state is handled by other means).
IN_PROC_BROWSER_TEST_F(SSLUITestBlock, TestBlockDisplayingInsecureImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server_.Start());

  std::string replacement_path;
  GetFilePathWithHostAndPortReplacement(
      "/ssl/page_displays_insecure_content.html",
      embedded_test_server()->host_port_pair(), &replacement_path);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_.GetURL(replacement_path));

  CheckAuthenticatedState(browser()->tab_strip_model()->GetActiveWebContents(),
                          AuthState::NONE);
}

// Test that when the browser blocks displaying insecure content (iframes), the
// indicator shows a secure page, because the blocking made the otherwise
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
// unsafe page safe (the notification of this state is handled by other means).
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

// Verifies that the interstitial can proceed, even if JavaScript is disabled.
// http://crbug.com/322948
#if defined(OS_LINUX)
// flaky http://crbug.com/396458
#define MAYBE_TestInterstitialJavaScriptProceeds \
    DISABLED_TestInterstitialJavaScriptProceeds
#else
#define MAYBE_TestInterstitialJavaScriptProceeds \
    TestInterstitialJavaScriptProceeds
#endif
IN_PROC_BROWSER_TEST_F(SSLUITest, MAYBE_TestInterstitialJavaScriptProceeds) {
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
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab->GetController()));
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_page->GetDelegateForTesting()->GetTypeForTesting());
  content::RenderViewHost* interstitial_rvh =
      interstitial_page->GetMainFrame()->GetRenderViewHost();
  int result = -1;
  std::string javascript =
      base::StringPrintf("window.domAutomationController.send(%d);",
                         security_interstitials::CMD_PROCEED);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
              interstitial_rvh, javascript, &result));
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
  std::string javascript =
      base::StringPrintf("window.domAutomationController.send(%d);",
                         security_interstitials::CMD_DONT_PROCEED);
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      interstitial_rvh, javascript, &result));
  // The above will hang without the fix.
  EXPECT_EQ(0, result);
  observer.Wait();
  EXPECT_EQ("about:blank", tab->GetVisibleURL().spec());
}

// Verifies that switching tabs, while showing interstitial page, will not
// affect the visibility of the interestitial.
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
      https_server_.GetURL("/ssl/google.html").host();
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
  ASSERT_NO_FATAL_FAILURE(CheckSSLStatusesEquals(after_interstitial_ssl_status,
                                                 interstitial_ssl_status));
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
  SSLErrorHandler::SetClockForTest(&mock_clock);
  ssl_errors::SetBuildTimeForTesting(base::Time::NowFromSystemTime());

  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
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
  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
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
  ASSERT_NO_FATAL_FAILURE(CheckSSLStatusesEquals(
      after_interstitial_ssl_status, clock_interstitial_ssl_status));
}

class CommonNameMismatchBrowserTest : public CertVerifierBrowserTest {
 public:
  CommonNameMismatchBrowserTest() : CertVerifierBrowserTest() {}
  ~CommonNameMismatchBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable finch experiment for SSL common name mismatch handling.
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    "SSLCommonNameMismatchHandling/Enabled/");
  }
};

// Visit the URL www.mail.example.com on a server that presents a valid
// certificate for mail.example.com. Verify that the page navigates to
// mail.example.com.
IN_PROC_BROWSER_TEST_F(CommonNameMismatchBrowserTest,
                       ShouldShowWWWSubdomainMismatchInterstitial) {
  net::EmbeddedTestServer https_server_example_domain_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain_.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain_.Start());

  host_resolver()->AddRule(
      "mail.example.com", https_server_example_domain_.host_port_pair().host());
  host_resolver()->AddRule(
      "www.mail.example.com",
      https_server_example_domain_.host_port_pair().host());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain_.GetCertificate();

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
  GURL https_server_url =
      https_server_example_domain_.GetURL("/ssl/google.html?a=b#anchor");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents, 2);
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  observer.Wait();

  CheckSecurityState(contents, CertError::NONE,
                     content::SECURITY_STYLE_AUTHENTICATED, AuthState::NONE);
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
  net::EmbeddedTestServer https_server_example_domain_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain_.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain_.Start());

  host_resolver()->AddRule(
      "www.example.org", https_server_example_domain_.host_port_pair().host());
  host_resolver()->AddRule(
      "example.org", https_server_example_domain_.host_port_pair().host());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain_.GetCertificate();

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

  GURL https_server_url =
      https_server_example_domain_.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("example.org");
  GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(contents, 2);
  ui_test_utils::NavigateToURL(browser(), https_server_mismatched_url);
  observer.Wait();

  CheckSecurityState(contents, CertError::NONE,
                     content::SECURITY_STYLE_AUTHENTICATED, AuthState::NONE);
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
  net::EmbeddedTestServer https_server_example_domain_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain_.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain_.Start());

  host_resolver()->AddRule(
      "mail.example.com", https_server_example_domain_.host_port_pair().host());
  host_resolver()->AddRule(
      "www.mail.example.com",
      https_server_example_domain_.host_port_pair().host());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain_.GetCertificate();

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

  GURL https_server_url =
      https_server_example_domain_.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTest(base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
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
  net::EmbeddedTestServer https_server_example_domain_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain_.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain_.Start());

  host_resolver()->AddRule(
      "mail.example.com", https_server_example_domain_.host_port_pair().host());
  host_resolver()->AddRule(
      "www.mail.example.com",
      https_server_example_domain_.host_port_pair().host());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain_.GetCertificate();

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

  GURL https_server_url =
      https_server_example_domain_.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTest(base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  chrome::Reload(browser(), CURRENT_TAB);
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
  net::EmbeddedTestServer https_server_example_domain_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_example_domain_.ServeFilesFromSourceDirectory(
      base::FilePath(kDocRoot));
  ASSERT_TRUE(https_server_example_domain_.Start());

  host_resolver()->AddRule(
      "mail.example.com", https_server_example_domain_.host_port_pair().host());
  host_resolver()->AddRule(
      "www.mail.example.com",
      https_server_example_domain_.host_port_pair().host());

  scoped_refptr<net::X509Certificate> cert =
      https_server_example_domain_.GetCertificate();

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

  GURL https_server_url =
      https_server_example_domain_.GetURL("/ssl/google.html?a=b");
  GURL::Replacements replacements;
  replacements.SetHostStr("www.mail.example.com");
  GURL https_server_mismatched_url =
      https_server_url.ReplaceComponents(replacements);

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CommonNameMismatchHandler::set_state_for_testing(
      CommonNameMismatchHandler::IGNORE_REQUESTS_FOR_TESTING);
  // Set delay long enough so that the page appears loading.
  SSLErrorHandler::SetInterstitialDelayForTest(base::TimeDelta::FromHours(1));
  SSLInterstitialTimerObserver interstitial_timer_observer(contents);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), https_server_mismatched_url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  EXPECT_TRUE(contents->IsLoading());
  content::TestNavigationObserver observer(contents, 1);
  browser()->OpenURL(content::OpenURLParams(GURL("https://google.com"),
                                            content::Referrer(), CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
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
  SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const override {
    net::SSLInfo ssl_info;
    ssl_info.cert = new net::X509Certificate(
        request_url.host(), "CA", base::Time::Max(), base::Time::Max());
    return new SSLBlockingPage(
        contents, net::ERR_CERT_CONTAINS_ERRORS, ssl_info, request_url, 0,
        base::Time::NowFromSystemTime(), nullptr, base::Callback<void(bool)>());
  }
};

IN_PROC_BROWSER_TEST_F(SSLBlockingPageIDNTest, SSLBlockingPageDecodesIDN) {
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
                     content::SECURITY_STYLE_AUTHENTICATION_BROKEN,
                     AuthState::SHOWING_INTERSTITIAL);
}

// TODO(jcampan): more tests to do below.

// Visit a page over https that contains a frame with a redirect.

// XMLHttpRequest insecure content in synchronous mode.

// XMLHttpRequest insecure content in asynchronous mode.

// XMLHttpRequest over bad ssl in synchronous mode.

// XMLHttpRequest over OK ssl in synchronous mode.
