// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class SSLErrorHandlerForTest : public SSLErrorHandler {
 public:
  SSLErrorHandlerForTest(Profile* profile,
                         content::WebContents* web_contents,
                         const net::SSLInfo& ssl_info)
      : SSLErrorHandler(web_contents,
                        net::ERR_CERT_COMMON_NAME_INVALID,
                        ssl_info,
                        GURL(),
                        0,
                        nullptr,
                        base::Callback<void(bool)>()),
        profile_(profile),
        captive_portal_checked_(false),
        suggested_url_exists_(false),
        suggested_url_checked_(false),
        ssl_interstitial_shown_(false),
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
  bool captive_portal_interstitial_shown_;
  bool redirected_to_suggested_url_;
  bool is_overridable_error_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandlerForTest);
};

class SSLErrorHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerTest() : field_trial_list_(nullptr) {}

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
};

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerTest,
       ShouldShowSSLInterstitialOnTimerExpired) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());

  error_handler()->ClearSeenOperations();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
}

TEST_F(SSLErrorHandlerTest,
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
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_FALSE(error_handler()->ssl_interstitial_shown());
  EXPECT_TRUE(error_handler()->captive_portal_interstitial_shown());
}

TEST_F(SSLErrorHandlerTest,
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
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
}

TEST_F(SSLErrorHandlerTest, ShouldNotCheckSuggestedUrlIfNoSuggestedUrl) {
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
}

TEST_F(SSLErrorHandlerTest, ShouldNotCheckCaptivePortalIfSuggestedUrlExists) {
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

TEST_F(SSLErrorHandlerTest, ShouldNotHandleNameMismatchOnNonOverridableError) {
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

#else  // #if !defined(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerTest,
       ShouldShowSSLInterstitialOnCaptivePortalDetectionDisabled) {
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
}

#endif  // defined(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerTest,
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

TEST_F(SSLErrorHandlerTest, ShouldRedirectOnSuggestedUrlCheckResult) {
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

TEST_F(SSLErrorHandlerTest, ShouldShowSSLInterstitialOnInvalidUrlCheckResult) {
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
