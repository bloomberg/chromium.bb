// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestSSLErrorHandler : public SSLErrorHandler {
 public:
  TestSSLErrorHandler(Profile* profile,
                      content::WebContents* web_contents,
                      const net::SSLInfo& ssl_info)
      : SSLErrorHandler(web_contents,
                        net::ERR_CERT_COMMON_NAME_INVALID,
                        ssl_info,
                        GURL(),
                        0,
                        base::Callback<void(bool)>()),
        profile_(profile),
        captive_portal_checked_(false),
        ssl_interstitial_shown_(false),
        captive_portal_interstitial_shown_(false) {
  }

  ~TestSSLErrorHandler() override {
  }

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

  bool IsTimerRunning() const {
    return get_timer().IsRunning();
  }

  int captive_portal_checked() const {
    return captive_portal_checked_;
  }

  int ssl_interstitial_shown() const {
    return ssl_interstitial_shown_;
  }

  int captive_portal_interstitial_shown() const {
    return captive_portal_interstitial_shown_;
  }

  void Reset() {
    captive_portal_checked_ = false;
    ssl_interstitial_shown_ = false;
    captive_portal_interstitial_shown_ = false;
  }

 private:
  void CheckForCaptivePortal() override {
    captive_portal_checked_ = true;
  }

  void ShowSSLInterstitial() override {
    ssl_interstitial_shown_ = true;
  }

  void ShowCaptivePortalInterstitial(const GURL& landing_url) override {
    captive_portal_interstitial_shown_ = true;
  }

  Profile* profile_;
  bool captive_portal_checked_;
  bool ssl_interstitial_shown_;
  bool captive_portal_interstitial_shown_;

  DISALLOW_COPY_AND_ASSIGN(TestSSLErrorHandler);
};

class SSLErrorHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorHandlerTest()
      : field_trial_list_(NULL) {
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SSLErrorHandler::SetInterstitialDelayTypeForTest(SSLErrorHandler::NONE);
    error_handler_.reset(new TestSSLErrorHandler(profile(),
                                                 web_contents(),
                                                 ssl_info_));
    // Enable finch experiment for captive portal interstitials.
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
                    "CaptivePortalInterstitial", "Enabled"));
}

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunning());
    error_handler_.reset(NULL);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }

 private:
  net::SSLInfo ssl_info_;
  scoped_ptr<TestSSLErrorHandler> error_handler_;
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

  error_handler()->Reset();
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
  error_handler()->Reset();
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
  error_handler()->Reset();
  error_handler()->SendCaptivePortalNotification(
      captive_portal::RESULT_INTERNET_CONNECTED);
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunning());
  EXPECT_FALSE(error_handler()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->captive_portal_interstitial_shown());
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
