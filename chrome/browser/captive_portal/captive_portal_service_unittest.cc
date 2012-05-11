// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captive_portal {

namespace {

// A short amount of time that some tests wait for.
const int kShortTimeMs = 10;

scoped_refptr<net::HttpResponseHeaders> CreateResponseHeaders(
    const std::string& response_headers) {
  std::string raw_headers =
    net::HttpUtil::AssembleRawHeaders(response_headers.c_str(),
                                      response_headers.length());
  return new net::HttpResponseHeaders(raw_headers);
}

// Allows setting the time, for testing timers.
class TestCaptivePortalService : public CaptivePortalService {
 public:
  // The initial time does not matter, except it must not be NULL.
  explicit TestCaptivePortalService(Profile* profile)
      : CaptivePortalService(profile),
        time_ticks_(base::TimeTicks::Now()),
        time_(base::Time::Now()) {
  }

  virtual ~TestCaptivePortalService() {}

  void AdvanceTime(base::TimeDelta delta) {
    time_ticks_ += delta;
    time_ += delta;
  }

  // CaptivePortalService:
  virtual base::TimeTicks GetCurrentTimeTicks() const OVERRIDE {
    return time_ticks_;
  }

  virtual base::Time GetCurrentTime() const OVERRIDE {
    return time_;
  }

  void set_time(base::Time time) {
    time_= time;
  }

 private:
  base::TimeTicks time_ticks_;

  // Not necessarily consistent with |time_ticks_|.  Used solely with
  // Retry-After dates.
  base::Time time_;

  DISALLOW_COPY_AND_ASSIGN(TestCaptivePortalService);
};

// An observer watches the CaptivePortalService.  It tracks the last
// received result and the total number of received results.
class CaptivePortalObserver : public content::NotificationObserver {
 public:
  CaptivePortalObserver(Profile* profile,
                        CaptivePortalService* captive_portal_service)
      : captive_portal_result_(
            captive_portal_service->last_detection_result()),
        num_results_received_(0),
        profile_(profile),
        captive_portal_service_(captive_portal_service) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                   content::Source<Profile>(profile_));
  }

  Result captive_portal_result() const { return captive_portal_result_; }

  int num_results_received() const { return num_results_received_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    ASSERT_EQ(type, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT);
    ASSERT_EQ(profile_, content::Source<Profile>(source).ptr());

    CaptivePortalService::Results *results =
        content::Details<CaptivePortalService::Results>(details).ptr();

    EXPECT_EQ(captive_portal_result_, results->previous_result);
    EXPECT_EQ(captive_portal_service_->last_detection_result(),
              results->result);

    captive_portal_result_ = results->result;
    ++num_results_received_;
  }

  Result captive_portal_result_;
  int num_results_received_;

  Profile* profile_;
  CaptivePortalService* captive_portal_service_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalObserver);
};

}  // namespace

class CaptivePortalServiceTest : public testing::Test {
 public:
  CaptivePortalServiceTest() {}

  virtual ~CaptivePortalServiceTest() {}

  void Initialize(bool enable_on_command_line) {
    if (enable_on_command_line) {
      CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kCaptivePortalDetection);
    }

    profile_.reset(new TestingProfile());
    service_.reset(new TestCaptivePortalService(profile_.get()));
    scoped_ptr<TestCaptivePortalService> service_;

    // Use no delays for most tests.
    set_initial_backoff_no_portal(base::TimeDelta());
    set_initial_backoff_portal(base::TimeDelta());

    // Disable jitter, so can check exact values.
    set_jitter_factor(0.0);

    // These values make checking exponential backoff easier.
    set_multiply_factor(2.0);
    set_maximum_backoff(base::TimeDelta::FromSeconds(1600));

    // This means backoff starts after the first "failure", which is the second
    // captive portal test in a row that ends up with the same result.
    set_num_errors_to_ignore(0);

    EnableCaptivePortalDetection(true);
  }

  // Sets the captive portal checking preference.
  void EnableCaptivePortalDetection(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kAlternateErrorPagesEnabled,
                                      enabled);
  }

  // Calls the corresponding CaptivePortalService function.
  void OnURLFetchComplete(content::URLFetcher* fetcher) {
    service()->OnURLFetchComplete(fetcher);
  }

  // Triggers a captive portal check, then simulates the URL request
  // returning with the specified |net_error| and |status_code|.  If |net_error|
  // is not OK, |status_code| is ignored.  Expects the CaptivePortalService to
  // return |expected_result|.
  //
  // |expected_delay_secs| is the expected value of GetTimeUntilNextRequest().
  // The function makes sure the value is as expected, and then simulates
  // waiting for that period of time before running the test.
  //
  // If |response_headers| is non-NULL, the response will use it as headers
  // for the simulate URL request.  It must use single linefeeds as line breaks.
  void RunTest(Result expected_result,
               int net_error,
               int status_code,
               int expected_delay_secs,
               const char* response_headers) {
    base::TimeDelta expected_delay =
        base::TimeDelta::FromSeconds(expected_delay_secs);

    ASSERT_EQ(CaptivePortalService::STATE_IDLE, service()->state());
    ASSERT_EQ(expected_delay, GetTimeUntilNextRequest());

    service()->AdvanceTime(expected_delay);
    ASSERT_EQ(base::TimeDelta(), GetTimeUntilNextRequest());

    CaptivePortalObserver observer(profile(), service());
    TestURLFetcherFactory factory;
    service()->DetectCaptivePortal();

    EXPECT_EQ(CaptivePortalService::STATE_TIMER_RUNNING, service()->state());
    EXPECT_FALSE(FetchingURL());
    ASSERT_TRUE(TimerRunning());

    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(CaptivePortalService::STATE_CHECKING_FOR_PORTAL,
              service()->state());
    ASSERT_TRUE(FetchingURL());
    EXPECT_FALSE(TimerRunning());

    TestURLFetcher* fetcher = factory.GetFetcherByID(0);
    if (net_error != net::OK) {
      EXPECT_FALSE(response_headers);
      fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                                net_error));
    } else {
      fetcher->set_response_code(status_code);
      if (response_headers) {
        scoped_refptr<net::HttpResponseHeaders> headers(
            CreateResponseHeaders(response_headers));
        // Sanity check.
        EXPECT_EQ(status_code, headers->response_code());
        fetcher->set_response_headers(headers);
      }
    }

    OnURLFetchComplete(fetcher);

    EXPECT_FALSE(FetchingURL());
    EXPECT_FALSE(TimerRunning());
    EXPECT_EQ(1, observer.num_results_received());
    EXPECT_EQ(expected_result, observer.captive_portal_result());
  }

  // Runs a test when the captive portal service is disabled.
  void RunDisabledTest(int expected_delay_secs) {
    base::TimeDelta expected_delay =
        base::TimeDelta::FromSeconds(expected_delay_secs);

    ASSERT_EQ(CaptivePortalService::STATE_IDLE, service()->state());
    ASSERT_EQ(expected_delay, GetTimeUntilNextRequest());

    service()->AdvanceTime(expected_delay);
    ASSERT_EQ(base::TimeDelta(), GetTimeUntilNextRequest());

    CaptivePortalObserver observer(profile(), service());
    service()->DetectCaptivePortal();

    EXPECT_EQ(CaptivePortalService::STATE_TIMER_RUNNING, service()->state());
    EXPECT_FALSE(FetchingURL());
    ASSERT_TRUE(TimerRunning());

    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(FetchingURL());
    EXPECT_FALSE(TimerRunning());
    EXPECT_EQ(1, observer.num_results_received());
    EXPECT_EQ(RESULT_INTERNET_CONNECTED, observer.captive_portal_result());
  }

  // Tests exponential backoff.  Prior to calling, the relevant recheck settings
  // must be set to have a minimum time of 100 seconds, with 2 checks before
  // starting exponential backoff.
  void RunBackoffTest(Result expected_result, int net_error, int status_code) {
    RunTest(expected_result, net_error, status_code, 0, NULL);
    RunTest(expected_result, net_error, status_code, 100, NULL);
    RunTest(expected_result, net_error, status_code, 200, NULL);
    RunTest(expected_result, net_error, status_code, 400, NULL);
    RunTest(expected_result, net_error, status_code, 800, NULL);
    RunTest(expected_result, net_error, status_code, 1600, NULL);
    RunTest(expected_result, net_error, status_code, 1600, NULL);
  }

  bool FetchingURL() {
    return service()->FetchingURL();
  }

  bool TimerRunning() {
    return service()->TimerRunning();
  }

  base::TimeDelta GetTimeUntilNextRequest() {
    return service()->backoff_entry_->GetTimeUntilRelease();
  }

  void set_initial_backoff_no_portal(
      base::TimeDelta initial_backoff_no_portal) {
    service()->recheck_policy().initial_backoff_no_portal_ms =
        initial_backoff_no_portal.InMilliseconds();
  }

  void set_initial_backoff_portal(base::TimeDelta initial_backoff_portal) {
    service()->recheck_policy().initial_backoff_portal_ms =
        initial_backoff_portal.InMilliseconds();
  }

  void set_maximum_backoff(base::TimeDelta maximum_backoff) {
    service()->recheck_policy().backoff_policy.maximum_backoff_ms =
        maximum_backoff.InMilliseconds();
  }

  void set_num_errors_to_ignore(int num_errors_to_ignore) {
    service()->recheck_policy().backoff_policy.num_errors_to_ignore =
        num_errors_to_ignore;
  }

  void set_multiply_factor(double multiply_factor) {
    service()->recheck_policy().backoff_policy.multiply_factor =
        multiply_factor;
  }

  void set_jitter_factor(double jitter_factor) {
    service()->recheck_policy().backoff_policy.jitter_factor = jitter_factor;
  }

  TestingProfile* profile() { return profile_.get(); }

  TestCaptivePortalService* service() { return service_.get(); }

 private:
  MessageLoop message_loop_;

  // Note that the construction order of these matters.
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<TestCaptivePortalService> service_;
};

// Test when connected to the Internet and get the expected 204 response.
TEST_F(CaptivePortalServiceTest, CaptivePortalInternetConnected) {
  Initialize(true);
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
}

// Test when there's a connection to the Internet, but the server returns a 5xx
// error.
TEST_F(CaptivePortalServiceTest, CaptivePortalServiceDown) {
  Initialize(true);
  RunTest(RESULT_NO_RESPONSE, net::OK, 500, 0, NULL);
}

// Test when there's a network error.
TEST_F(CaptivePortalServiceTest, CaptivePortalNetError) {
  Initialize(true);
  RunTest(RESULT_NO_RESPONSE, net::ERR_TIMED_OUT, -1, 0, NULL);
}

// Test when behind a captive portal.
TEST_F(CaptivePortalServiceTest, CaptivePortalBehindCaptivePortal) {
  Initialize(true);
  RunTest(RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 0, NULL);
}

// Verify that an observer doesn't get messages from the wrong profile.
TEST_F(CaptivePortalServiceTest, CaptivePortalTwoProfiles) {
  Initialize(true);
  TestingProfile profile2;
  TestCaptivePortalService service2(&profile2);
  CaptivePortalObserver observer2(&profile2, &service2);

  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
  EXPECT_EQ(0, observer2.num_results_received());
}

// Test receiving two results in a row, with no timeout.
TEST_F(CaptivePortalServiceTest, CaptivePortalTwoResults) {
  Initialize(true);
  CaptivePortalObserver observer(profile(), service());

  RunTest(RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 0, NULL);
  EXPECT_EQ(RESULT_BEHIND_CAPTIVE_PORTAL, observer.captive_portal_result());
  EXPECT_EQ(1, observer.num_results_received());

  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
  EXPECT_EQ(RESULT_INTERNET_CONNECTED, observer.captive_portal_result());
  EXPECT_EQ(2, observer.num_results_received());
}

// Checks exponential backoff when the Internet is connected.
TEST_F(CaptivePortalServiceTest, CaptivePortalRecheckInternetConnected) {
  Initialize(true);

  // This value should have no effect on this test, until the end.
  set_initial_backoff_portal(base::TimeDelta::FromSeconds(1));

  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  RunBackoffTest(RESULT_INTERNET_CONNECTED, net::OK, 204);

  // Make sure that getting a new result resets the timer.
  RunTest(RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 1600, NULL);
  RunTest(RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 1, NULL);
}

// Checks exponential backoff when there's an HTTP error.
TEST_F(CaptivePortalServiceTest, CaptivePortalRecheckError) {
  Initialize(true);

  // This value should have no effect on this test.
  set_initial_backoff_portal(base::TimeDelta::FromDays(1));

  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  RunBackoffTest(RESULT_NO_RESPONSE, net::OK, 500);

  // Make sure that getting a new result resets the timer.
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 1600, NULL);
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 100, NULL);
}

// Checks exponential backoff when there's a captive portal.
TEST_F(CaptivePortalServiceTest, CaptivePortalRecheckBehindPortal) {
  Initialize(true);

  // This value should have no effect on this test, until the end.
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(250));

  set_initial_backoff_portal(base::TimeDelta::FromSeconds(100));
  RunBackoffTest(RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200);

  // Make sure that getting a new result resets the timer.
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 1600, NULL);
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 250, NULL);
}

// Check that everything works as expected when captive portal checking is
// disabled, including throttling.  Then enables it again and runs another test.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefDisabled) {
  Initialize(true);

  // This value should have no effect on this test.
  set_initial_backoff_no_portal(base::TimeDelta::FromDays(1));

  set_initial_backoff_portal(base::TimeDelta::FromSeconds(100));

  EnableCaptivePortalDetection(false);

  RunDisabledTest(0);
  for (int i = 0; i < 6; ++i)
    RunDisabledTest(100);

  EnableCaptivePortalDetection(true);

  RunTest(RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 0, NULL);
}

// Check that disabling the captive portal service while a check is running
// works.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefDisabledWhileRunning) {
  Initialize(true);
  CaptivePortalObserver observer(profile(), service());

  // Needed to create the URLFetcher, even if it never returns any results.
  TestURLFetcherFactory factory;
  service()->DetectCaptivePortal();

  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(FetchingURL());
  EXPECT_FALSE(TimerRunning());

  EnableCaptivePortalDetection(false);
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());
  EXPECT_EQ(0, observer.num_results_received());

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(FetchingURL());
  EXPECT_FALSE(TimerRunning());
  EXPECT_EQ(1, observer.num_results_received());

  EXPECT_EQ(RESULT_INTERNET_CONNECTED, observer.captive_portal_result());
}

// Check that disabling the captive portal service while a check is pending
// works.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefDisabledWhilePending) {
  Initialize(true);
  set_initial_backoff_no_portal(base::TimeDelta::FromDays(1));

  // Needed to create the URLFetcher, even if it never returns any results.
  TestURLFetcherFactory factory;

  CaptivePortalObserver observer(profile(), service());
  service()->DetectCaptivePortal();
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());

  EnableCaptivePortalDetection(false);
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());
  EXPECT_EQ(0, observer.num_results_received());

  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(FetchingURL());
  EXPECT_FALSE(TimerRunning());
  EXPECT_EQ(1, observer.num_results_received());

  EXPECT_EQ(RESULT_INTERNET_CONNECTED, observer.captive_portal_result());
}

// Check that disabling the captive portal service while a check is pending
// works.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefEnabledWhilePending) {
  Initialize(true);

  EnableCaptivePortalDetection(false);
  RunDisabledTest(0);

  CaptivePortalObserver observer(profile(), service());
  service()->DetectCaptivePortal();
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());

  TestURLFetcherFactory factory;

  EnableCaptivePortalDetection(true);
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());

  MessageLoop::current()->RunAllPending();
  ASSERT_TRUE(FetchingURL());
  EXPECT_FALSE(TimerRunning());

  TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  fetcher->set_response_code(200);
  OnURLFetchComplete(fetcher);
  EXPECT_FALSE(FetchingURL());
  EXPECT_FALSE(TimerRunning());

  EXPECT_EQ(1, observer.num_results_received());
  EXPECT_EQ(RESULT_BEHIND_CAPTIVE_PORTAL, observer.captive_portal_result());
}

// Checks that disabling with a command line flag works as expected.
TEST_F(CaptivePortalServiceTest, CaptivePortalDisabledAtCommandLine) {
  Initialize(false);
  RunDisabledTest(0);
}

// Checks that jitter gives us values in the correct range.
TEST_F(CaptivePortalServiceTest, CaptivePortalJitter) {
  Initialize(true);
  set_jitter_factor(0.3);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);

  for (int i = 0; i < 50; ++i) {
    int interval_sec = GetTimeUntilNextRequest().InSeconds();
    // Allow for roundoff, though shouldn't be necessary.
    EXPECT_LE(69, interval_sec);
    EXPECT_LE(interval_sec, 101);
  }
}

// Check a Retry-After header that contains a delay in seconds.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterSeconds) {
  Initialize(true);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));

  RunTest(RESULT_NO_RESPONSE,
          net::OK,
          503,
          0,
          "HTTP/1.1 503 OK\nRetry-After: 101\n\n");

  // Run another captive portal check to make sure the time until the next check
  // is as expected.
  RunTest(RESULT_INTERNET_CONNECTED, net::OK, 204, 101, NULL);
  EXPECT_EQ(base::TimeDelta::FromSeconds(100), GetTimeUntilNextRequest());
}

// Check that the RecheckPolicy is still respected on 503 responses with
// Retry-After headers.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterSecondsTooShort) {
  Initialize(true);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));

  RunTest(RESULT_NO_RESPONSE,
          net::OK,
          503,
          0,
          "HTTP/1.1 503 OK\nRetry-After: 99\n\n");
  EXPECT_EQ(base::TimeDelta::FromSeconds(100), GetTimeUntilNextRequest());
}

// Check a Retry-After header that contains a date.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterDate) {
  Initialize(true);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(50));

  // base has a function to get a time in the right format from a string, but
  // not the other way around.
  base::Time start_time;
  ASSERT_TRUE(
      base::Time::FromString("Tue, 17 Apr 2012 18:02:00 GMT", &start_time));
  service()->set_time(start_time);

  RunTest(RESULT_NO_RESPONSE,
          net::OK,
          503,
          0,
          "HTTP/1.1 503 OK\nRetry-After: Tue, 17 Apr 2012 18:02:51 GMT\n\n");
  EXPECT_EQ(base::TimeDelta::FromSeconds(51), GetTimeUntilNextRequest());
}

// Check invalid Retry-After headers are ignored.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterInvalid) {
  Initialize(true);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));

  RunTest(RESULT_NO_RESPONSE,
          net::OK,
          503,
          0,
          "HTTP/1.1 503 OK\nRetry-After: Christmas\n\n");
  EXPECT_EQ(base::TimeDelta::FromSeconds(100), GetTimeUntilNextRequest());
}

}  // namespace captive_portal
