// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"

namespace captive_portal {

namespace {

// The test URL.  When connected to the Internet, it should return a blank page
// with a 204 status code.  When behind a captive portal, requests for this
// URL should get an HTTP redirect or a login page.  When neither is true,
// no server should respond to requests for this URL.
// TODO(mmenke):  Look into getting another domain, for better load management.
//                Need a cookieless domain, so can't use clients*.google.com.
const char* const kDefaultTestURL = "http://www.gstatic.com/generate_204";

// Used for histograms.
const char* const kCaptivePortalResultNames[] = {
  "InternetConnected",
  "NoResponse",
  "BehindCaptivePortal",
  "NumCaptivePortalResults",
};
COMPILE_ASSERT(arraysize(kCaptivePortalResultNames) == RESULT_COUNT + 1,
               captive_portal_result_name_count_mismatch);

// Used for histograms.
std::string CaptivePortalResultToString(Result result) {
  DCHECK_GE(result, 0);
  DCHECK_LT(static_cast<unsigned int>(result),
            arraysize(kCaptivePortalResultNames));
  return kCaptivePortalResultNames[result];
}

// Records histograms relating to how often captive portal detection attempts
// ended with |result| in a row, and for how long |result| was the last result
// of a detection attempt.  Recorded both on quit and on a new Result.
//
// |repeat_count| may be 0 if there were no captive portal checks during
// a session.
//
// |result_duration| is the time between when a captive portal check first
// returned |result| and when a check returned a different result, or when the
// CaptivePortalService was shut down.
void RecordRepeatHistograms(Result result,
                            int repeat_count,
                            base::TimeDelta result_duration) {
  // Histogram macros can't be used with variable names, since they cache
  // pointers, so have to use the histogram functions directly.

  // Record number of times the last result was received in a row.
  base::Histogram* result_repeated_histogram =
      base::Histogram::FactoryGet(
          "CaptivePortal.ResultRepeated." +
              CaptivePortalResultToString(result),
          1,  // min
          100,  // max
          100,  // bucket_count
          base::Histogram::kUmaTargetedHistogramFlag);
  result_repeated_histogram->Add(repeat_count);

  if (repeat_count == 0)
    return;

  // Time between first request that returned |result| and now.
  base::Histogram* result_duration_histogram =
      base::Histogram::FactoryTimeGet(
          "CaptivePortal.ResultDuration." +
              CaptivePortalResultToString(result),
          base::TimeDelta::FromSeconds(1),  // min
          base::TimeDelta::FromHours(1),  // max
          50,  // bucket_count
          base::Histogram::kUmaTargetedHistogramFlag);
  result_duration_histogram->AddTime(result_duration);
}

}  // namespace

class CaptivePortalService::RecheckBackoffEntry : public net::BackoffEntry {
 public:
  explicit RecheckBackoffEntry(CaptivePortalService* captive_portal_service)
      : net::BackoffEntry(
            &captive_portal_service->recheck_policy().backoff_policy),
        captive_portal_service_(captive_portal_service) {
  }

 private:
  virtual base::TimeTicks ImplGetTimeNow() const OVERRIDE {
    return captive_portal_service_->GetCurrentTimeTicks();
  }

  CaptivePortalService* captive_portal_service_;

  DISALLOW_COPY_AND_ASSIGN(RecheckBackoffEntry);
};

CaptivePortalService::RecheckPolicy::RecheckPolicy()
    : initial_backoff_no_portal_ms(600 * 1000),
      initial_backoff_portal_ms(20 * 1000) {
  // Receiving a new Result is considered a success.  All subsequent requests
  // that get the same Result are considered "failures", so a value of N
  // means exponential backoff starts after getting a result N + 2 times:
  // +1 for the initial success, and +1 because N failures are ignored.
  //
  // A value of 6 means to start backoff on the 7th failure, which is the 8th
  // time the same result is received.
  backoff_policy.num_errors_to_ignore = 6;

  // It doesn't matter what this is initialized to.  It will be overwritten
  // after the first captive portal detection request.
  backoff_policy.initial_delay_ms = initial_backoff_no_portal_ms;

  backoff_policy.multiply_factor = 2.0;
  backoff_policy.jitter_factor = 0.3;
  backoff_policy.maximum_backoff_ms = 2 * 60 * 1000;

  // -1 means the entry never expires.  This doesn't really matter, as the
  // service never checks for its expiration.
  backoff_policy.entry_lifetime_ms = -1;

  backoff_policy.always_use_initial_delay = true;
}

CaptivePortalService::CaptivePortalService(Profile* profile)
    : profile_(profile),
      state_(STATE_IDLE),
      enabled_(false),
      last_detection_result_(RESULT_INTERNET_CONNECTED),
      num_checks_with_same_result_(0),
      test_url_(kDefaultTestURL) {
  // The order matters here:
  // |resolve_errors_with_web_service_| must be initialized and |backoff_entry_|
  // created before the call to UpdateEnabledState.
  resolve_errors_with_web_service_.Init(
      prefs::kAlternateErrorPagesEnabled,
      profile_->GetPrefs(),
      this);
  ResetBackoffEntry(last_detection_result_);

  UpdateEnabledState();
}

CaptivePortalService::~CaptivePortalService() {
}

void CaptivePortalService::DetectCaptivePortal() {
  DCHECK(CalledOnValidThread());

  // If a request is pending or running, do nothing.
  if (state_ == STATE_CHECKING_FOR_PORTAL || state_ == STATE_TIMER_RUNNING)
    return;

  base::TimeDelta time_until_next_check = backoff_entry_->GetTimeUntilRelease();

  // Start asynchronously.
  state_ = STATE_TIMER_RUNNING;
  check_captive_portal_timer_.Start(
      FROM_HERE,
      time_until_next_check,
      this,
      &CaptivePortalService::DetectCaptivePortalInternal);
}

void CaptivePortalService::DetectCaptivePortalInternal() {
  DCHECK(CalledOnValidThread());
  DCHECK(state_ == STATE_TIMER_RUNNING || state_ == STATE_IDLE);
  DCHECK(!FetchingURL());
  DCHECK(!TimerRunning());

  state_ = STATE_CHECKING_FOR_PORTAL;

  // When not enabled, just claim there's an Internet connection.
  if (!enabled_) {
    // Count this as a success, so the backoff entry won't apply exponential
    // backoff, but will apply the standard delay.
    backoff_entry_->InformOfRequest(true);
    OnResult(RESULT_INTERNET_CONNECTED);
    return;
  }

  // The first 0 means this can use a TestURLFetcherFactory in unit tests.
  url_fetcher_.reset(content::URLFetcher::Create(0,
                                                 test_url_,
                                                 net::URLFetcher::GET,
                                                 this));
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetRequestContext(profile_->GetRequestContext());
  // Can't safely use net::LOAD_DISABLE_CERT_REVOCATION_CHECKING here,
  // since then the connection may be reused without checking the cert.
  url_fetcher_->SetLoadFlags(
      net::LOAD_BYPASS_CACHE |
      net::LOAD_DO_NOT_PROMPT_FOR_LOGIN |
      net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
}

void CaptivePortalService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(STATE_CHECKING_FOR_PORTAL, state_);
  DCHECK(FetchingURL());
  DCHECK(!TimerRunning());
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(enabled_);

  base::TimeTicks now = GetCurrentTimeTicks();

  base::TimeDelta retry_after_delta;
  Result new_result = GetCaptivePortalResultFromResponse(source,
                                                         &retry_after_delta);
  url_fetcher_.reset();

  // Record histograms.
  UMA_HISTOGRAM_ENUMERATION("CaptivePortal.DetectResult",
                            new_result,
                            RESULT_COUNT);

  // If this isn't the first captive portal result, record stats.
  if (!last_check_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("CaptivePortal.TimeBetweenChecks",
                             now - last_check_time_);

    if (last_detection_result_ != new_result) {
      // If the last result was different from the result of the latest test,
      // record histograms about the previous period over which the result was
      // the same.
      RecordRepeatHistograms(last_detection_result_,
                             num_checks_with_same_result_,
                             now - first_check_time_with_same_result_);
    }
  }

  if (last_check_time_.is_null() || new_result != last_detection_result_) {
    first_check_time_with_same_result_ = now;
    num_checks_with_same_result_ = 1;

    // Reset the backoff entry both to update the default time and clear
    // previous failures.
    ResetBackoffEntry(new_result);

    backoff_entry_->SetCustomReleaseTime(now + retry_after_delta);
    backoff_entry_->InformOfRequest(true);
  } else {
    DCHECK_LE(1, num_checks_with_same_result_);
    ++num_checks_with_same_result_;

    // Requests that have the same Result as the last one are considered
    // "failures", to trigger backoff.
    backoff_entry_->InformOfRequest(false);
  }

  last_check_time_ = now;

  OnResult(new_result);
}

void CaptivePortalService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(chrome::NOTIFICATION_PREF_CHANGED, type);
  DCHECK_EQ(std::string(prefs::kAlternateErrorPagesEnabled),
            *content::Details<std::string>(details).ptr());
  UpdateEnabledState();
}

void CaptivePortalService::Shutdown() {
  DCHECK(CalledOnValidThread());
  if (enabled_) {
    RecordRepeatHistograms(
        last_detection_result_,
        num_checks_with_same_result_,
        GetCurrentTimeTicks() - first_check_time_with_same_result_);
  }
}

void CaptivePortalService::OnResult(Result result) {
  DCHECK_EQ(STATE_CHECKING_FOR_PORTAL, state_);
  state_ = STATE_IDLE;

  Results results;
  results.previous_result = last_detection_result_;
  results.result = result;
  last_detection_result_ = result;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
      content::Source<Profile>(profile_),
      content::Details<Results>(&results));
}

void CaptivePortalService::ResetBackoffEntry(Result result) {
  if (!enabled_ || result == RESULT_BEHIND_CAPTIVE_PORTAL) {
    // Use the shorter time when the captive portal service is not enabled, or
    // behind a captive portal.
    recheck_policy_.backoff_policy.initial_delay_ms =
        recheck_policy_.initial_backoff_portal_ms;
  } else {
    recheck_policy_.backoff_policy.initial_delay_ms =
        recheck_policy_.initial_backoff_no_portal_ms;
  }

  backoff_entry_.reset(new RecheckBackoffEntry(this));
}

void CaptivePortalService::UpdateEnabledState() {
  bool enabled_before = enabled_;
  enabled_ = resolve_errors_with_web_service_.GetValue() &&
             CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kCaptivePortalDetection);
  if (enabled_before == enabled_)
    return;

  // Clear data used for histograms.
  num_checks_with_same_result_ = 0;
  first_check_time_with_same_result_ = base::TimeTicks();
  last_check_time_ = base::TimeTicks();

  ResetBackoffEntry(last_detection_result_);

  if (state_ == STATE_CHECKING_FOR_PORTAL || state_ == STATE_TIMER_RUNNING) {
    // If a captive portal check was running or pending, stop the check or the
    // timer.
    url_fetcher_.reset();
    check_captive_portal_timer_.Stop();
    state_ = STATE_IDLE;

    // Since a captive portal request was queued or running, something may be
    // expecting to receive a captive portal result.
    DetectCaptivePortal();
  }
}

// Takes a content::URLFetcher that has finished trying to retrieve the test
// URL, and returns a CaptivePortalService::Result based on its result.
Result CaptivePortalService::GetCaptivePortalResultFromResponse(
    const net::URLFetcher* url_fetcher,
    base::TimeDelta* retry_after) const {
  DCHECK(retry_after);
  DCHECK(!url_fetcher->GetStatus().is_io_pending());

  *retry_after = base::TimeDelta();

  // If there's a network error of some sort when fetching a file via HTTP,
  // there may be a networking problem, rather than a captive portal.
  // TODO(mmenke):  Consider special handling for redirects that end up at
  //                errors, especially SSL certificate errors.
  if (url_fetcher->GetStatus().status() != net::URLRequestStatus::SUCCESS)
    return RESULT_NO_RESPONSE;

  // In the case of 503 errors, look for the Retry-After header.
  int response_code = url_fetcher->GetResponseCode();
  if (response_code == 503) {
    net::HttpResponseHeaders* headers = url_fetcher->GetResponseHeaders();
    std::string retry_after_string;

    // If there's no Retry-After header, nothing else to do.
    if (!headers->EnumerateHeader(NULL, "Retry-After", &retry_after_string))
      return RESULT_NO_RESPONSE;

    // Otherwise, try parsing it as an integer (seconds) or as an HTTP date.
    int seconds;
    base::Time full_date;
    if (base::StringToInt(retry_after_string, &seconds)) {
      *retry_after = base::TimeDelta::FromSeconds(seconds);
    } else if (headers->GetTimeValuedHeader("Retry-After", &full_date)) {
      base::Time now = GetCurrentTime();
      if (full_date > now)
        *retry_after = full_date - now;
    }
    return RESULT_NO_RESPONSE;
  }

  // Non-2xx/3xx HTTP responses may also indicate server errors.
  if (response_code >= 400 || response_code < 200)
    return RESULT_NO_RESPONSE;

  // A 204 response code indicates there's no captive portal.
  if (response_code == 204)
    return RESULT_INTERNET_CONNECTED;

  // Otherwise, assume it's a captive portal.
  return RESULT_BEHIND_CAPTIVE_PORTAL;
}

base::Time CaptivePortalService::GetCurrentTime() const {
  return base::Time::Now();
}

base::TimeTicks CaptivePortalService::GetCurrentTimeTicks() const {
  return base::TimeTicks::Now();
}

bool CaptivePortalService::FetchingURL() const {
  return url_fetcher_.get() != NULL;
}

bool CaptivePortalService::TimerRunning() const {
  return check_captive_portal_timer_.IsRunning();
}

}  // namespace captive_portal
