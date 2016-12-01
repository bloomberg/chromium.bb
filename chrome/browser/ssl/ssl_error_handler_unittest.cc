// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/common/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class SSLErrorHandlerForTest : public SSLErrorHandler {
 public:
  SSLErrorHandlerForTest(Profile* profile,
                         content::WebContents* web_contents,
                         const net::SSLInfo& ssl_info)
      : SSLErrorHandler(
            web_contents,
            net::MapCertStatusToNetError(ssl_info.cert_status),
            ssl_info,
            GURL(),
            0,
            nullptr,
            base::Callback<void(content::CertificateRequestResultType)>()),
        profile_(profile),
        captive_portal_checked_(false),
        suggested_url_exists_(false),
        suggested_url_checked_(false),
        ssl_interstitial_shown_(false),
        bad_clock_interstitial_shown_(false),
        captive_portal_interstitial_shown_(false),
        redirected_to_suggested_url_(false),
        is_overridable_error_(true) {}

  using SSLErrorHandler::StartHandlingError;

  void SendCaptivePortalNotification(
      captive_portal::CaptivePortalResult result) {
    CaptivePortalService::Results results;
    results.previous_result = captive_portal::RESULT_INTERNET_CONNECTED;
    results.result = result;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
        content::Source<Profile>(profile_),
        content::Details<CaptivePortalService::Results>(&results));
  }

  void SendSuggestedUrlCheckResult(
      const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
      const GURL& suggested_url) {
    CommonNameMismatchHandlerCallback(result, suggested_url);
  }

  bool IsTimerRunning() const { return get_timer().IsRunning(); }
  int captive_portal_checked() const { return captive_portal_checked_; }
  int ssl_interstitial_shown() const { return ssl_interstitial_shown_; }
  int captive_portal_interstitial_shown() const {
    return captive_portal_interstitial_shown_;
  }
  bool bad_clock_interstitial_shown() const {
    return bad_clock_interstitial_shown_;
  }
  bool suggested_url_checked() const { return suggested_url_checked_; }
  bool redirected_to_suggested_url() const {
    return redirected_to_suggested_url_;
  }

  void set_suggested_url_exists() { suggested_url_exists_ = true; }
  void set_non_overridable_error() { is_overridable_error_ = false; }

  void ClearSeenOperations() {
    captive_portal_checked_ = false;
    suggested_url_exists_ = false;
    suggested_url_checked_ = false;
    ssl_interstitial_shown_ = false;
    bad_clock_interstitial_shown_ = false;
    captive_portal_interstitial_shown_ = false;
    redirected_to_suggested_url_ = false;
  }

 private:
  void CheckForCaptivePortal() override {
    captive_portal_checked_ = true;
  }

  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override {
    if (!suggested_url_exists_)
      return false;
    *suggested_url = GURL("www.example.com");
    return true;
  }

  void ShowSSLInterstitial() override { ssl_interstitial_shown_ = true; }

  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override {
    bad_clock_interstitial_shown_ = true;
  }

  void ShowCaptivePortalInterstitial(const GURL& landing_url) override {
    captive_portal_interstitial_shown_ = true;
  }

  void CheckSuggestedUrl(const GURL& suggested_url) override {
    suggested_url_checked_ = true;
  }

  void NavigateToSuggestedURL(const GURL& suggested_url) override {
    redirected_to_suggested_url_ = true;
  }

  bool IsErrorOverridable() const override { return is_overridable_error_; }

  Profile* profile_;
  bool captive_portal_checked_;
  bool suggested_url_exists_;
  bool suggested_url_checked_;
  bool ssl_interstitial_shown_;
  bool bad_clock_interstitial_shown_;
  bool captive_portal_interstitial_shown_;
  bool redirected_to_suggested_url_;
  bool is_overridable_error_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerForTest);
};

class SSLErrorHandlerNameMismatchTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerNameMismatchTest() : field_trial_list_(nullptr) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::SetInterstitialDelayForTest(base::TimeDelta());
    ssl_info_.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ssl_info_.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    error_handler_.reset(
        new SSLErrorHandlerForTest(profile(), web_contents(), ssl_info_));
    // Enable finch experiment for captive portal interstitials.
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
                    "CaptivePortalInterstitial", "Enabled"));
    // Enable finch experiment for SSL common name mismatch handling.
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
                    "SSLCommonNameMismatchHandling", "Enabled"));
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunning());
    error_handler_.reset(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SSLErrorHandlerForTest* error_handler() { return error_handler_.get(); }

 private:
  net::SSLInfo ssl_info_;
  std::unique_ptr<SSLErrorHandlerForTest> error_handler_;
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerNameMismatchTest);
};

class SSLErrorHandlerDateInvalidTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerDateInvalidTest()
      : field_trial_test_(network_time::FieldTrialTest::CreateForUnitTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock),
        test_server_(new net::EmbeddedTestServer) {
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
    network_time::NetworkTimeTracker::RegisterPrefs(pref_service_.registry());
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    field_trial_test()->SetNetworkQueriesWithVariationsService(
        false, 0.0,
        network_time::NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);
    tracker_.reset(new network_time::NetworkTimeTracker(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_,
        new net::TestURLRequestContextGetter(
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::IO))));

    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::TimeDelta::FromDays(111));
    tick_clock_->Advance(base::TimeDelta::FromDays(222));

    SSLErrorHandler::SetInterstitialDelayForTest(base::TimeDelta());
    ssl_info_.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ssl_info_.cert_status = net::CERT_STATUS_DATE_INVALID;
    error_handler_.reset(
        new SSLErrorHandlerForTest(profile(), web_contents(), ssl_info_));
    error_handler_->SetNetworkTimeTrackerForTest(tracker_.get());

    // Fix flakiness in case system time is off and triggers a bad clock
    // interstitial. https://crbug.com/666821#c50
    ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  }

  void TearDown() override {
    if (error_handler()) {
      EXPECT_FALSE(error_handler()->IsTimerRunning());
      error_handler_.reset(nullptr);
    }
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SSLErrorHandlerForTest* error_handler() { return error_handler_.get(); }

  network_time::FieldTrialTest* field_trial_test() {
    return field_trial_test_.get();
  }

  network_time::NetworkTimeTracker* tracker() { return tracker_.get(); }

  net::EmbeddedTestServer* test_server() { return test_server_.get(); }

  void ClearErrorHandler() { error_handler_.reset(nullptr); }

 private:
  net::SSLInfo ssl_info_;
  std::unique_ptr<SSLErrorHandlerForTest> error_handler_;
  std::unique_ptr<network_time::FieldTrialTest> field_trial_test_;
  base::SimpleTestClock* clock_;
  base::SimpleTestTickClock* tick_clock_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<network_time::NetworkTimeTracker> tracker_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerDateInvalidTest);
};

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpired) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());

  error_handler()->ClearSeenOperations();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowCustomInterstitialOnCaptivePortalResult) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
  // Fake a captive portal result.
  error_handler()->ClearSeenOperations();
  error_handler()->SendCaptivePortalNotification(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_TRUE(error_handler()->captive_portal_interstitial_shown());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnNoCaptivePortalResult) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
  // Fake a "connected to internet" result for the captive portal check.
  // This should immediately trigger an SSL interstitial without waiting for
  // the timer to expire.
  error_handler()->ClearSeenOperations();
  error_handler()->SendCaptivePortalNotification(
      captive_portal::RESULT_INTERNET_CONNECTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckSuggestedUrlIfNoSuggestedUrl) {
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckCaptivePortalIfSuggestedUrlExists) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->suggested_url_checked());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotHandleNameMismatchOnNonOverridableError) {
  error_handler()->set_non_overridable_error();
  error_handler()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(error_handler()->suggested_url_checked());
  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunning());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
}

#else  // #if !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnCaptivePortalDetectionDisabled) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpiredWhenSuggestedUrlExists) {
  error_handler()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->suggested_url_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->redirected_to_suggested_url());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->redirected_to_suggested_url());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldRedirectOnSuggestedUrlCheckResult) {
  error_handler()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->suggested_url_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->redirected_to_suggested_url());
  // Fake a valid suggested URL check result.
  // The URL returned by |SuggestedUrlCheckResult| can be different from
  // |suggested_url|, if there is a redirect.
  error_handler()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_AVAILABLE,
      GURL("https://random.example.com"));

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_TRUE(error_handler()->redirected_to_suggested_url());
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnInvalidUrlCheckResult) {
  error_handler()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->suggested_url_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->redirected_to_suggested_url());
  // Fake an Invalid Suggested URL Check result.
  error_handler()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_NOT_AVAILABLE,
      GURL());

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->redirected_to_suggested_url());
}

TEST_F(SSLErrorHandlerDateInvalidTest, TimeQueryStarted) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. A bad clock interstitial
  // should be shown.
  test_server()->RegisterRequestHandler(
      base::Bind(&network_time::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  tracker()->WaitForFetchForTesting(123123123);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(error_handler()->bad_clock_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunning());
}

// Tests that an SSL interstitial is shown if the accuracy of the system
// clock can't be determined because network time is unavailable.
TEST_F(SSLErrorHandlerDateInvalidTest, NoTimeQueries) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Handle the error without enabling time queries. A bad clock interstitial
  // should not be shown.
  error_handler()->StartHandlingError();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->bad_clock_interstitial_shown());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
}

// Runs |quit_closure| on the UI thread once a URL request has been
// seen. Returns a request that hangs.
std::unique_ptr<net::test_server::HttpResponse> WaitForRequest(
    const base::Closure& quit_closure,
    const net::test_server::HttpRequest& request) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   quit_closure);
  return base::MakeUnique<net::test_server::HungResponse>();
}

// Tests that an SSL interstitial is shown if determing the accuracy of
// the system clock times out (e.g. because a network time query hangs).
TEST_F(SSLErrorHandlerDateInvalidTest, TimeQueryHangs) {
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. Because the
  // network time cannot be determined before the timer elapses, an SSL
  // interstitial should be shown.
  base::RunLoop wait_for_time_query_loop;
  test_server()->RegisterRequestHandler(
      base::Bind(&WaitForRequest, wait_for_time_query_loop.QuitClosure()));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetNetworkQueriesWithVariationsService(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();
  EXPECT_TRUE(error_handler()->IsTimerRunning());
  wait_for_time_query_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->bad_clock_interstitial_shown());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunning());

  // Clear the error handler to test that, when the request completes,
  // it doesn't try to call a callback on a deleted SSLErrorHandler.
  ClearErrorHandler();

  // Shut down the server to cancel the pending request.
  ASSERT_TRUE(test_server()->ShutdownAndWaitUntilComplete());
}
