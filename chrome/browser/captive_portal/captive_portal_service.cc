// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace captive_portal {

namespace {

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
  base::HistogramBase* result_repeated_histogram =
      base::Histogram::FactoryGet(
          "CaptivePortal.ResultRepeated." +
              CaptivePortalDetector::CaptivePortalResultToString(result),
          1,  // min
          100,  // max
          100,  // bucket_count
          base::Histogram::kUmaTargetedHistogramFlag);
  result_repeated_histogram->Add(repeat_count);

  if (repeat_count == 0)
    return;

  // Time between first request that returned |result| and now.
  base::HistogramBase* result_duration_histogram =
      base::Histogram::FactoryTimeGet(
          "CaptivePortal.ResultDuration." +
              CaptivePortalDetector::CaptivePortalResultToString(result),
          base::TimeDelta::FromSeconds(1),  // min
          base::TimeDelta::FromHours(1),  // max
          50,  // bucket_count
          base::Histogram::kUmaTargetedHistogramFlag);
  result_duration_histogram->AddTime(result_duration);
}

bool HasNativeCaptivePortalDetection() {
  // Lion and Windows 8 have their own captive portal detection that will open
  // a browser window as needed.
#if defined(OS_MACOSX)
  return base::mac::IsOSLionOrLater();
#elif defined(OS_WIN)
  return base::win::GetVersion() >= base::win::VERSION_WIN8;
#else
  return false;
#endif
}

}  // namespace

CaptivePortalService::TestingState CaptivePortalService::testing_state_ =
    NOT_TESTING;

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
      captive_portal_detector_(profile->GetRequestContext()),
      enabled_(false),
      last_detection_result_(RESULT_INTERNET_CONNECTED),
      num_checks_with_same_result_(0),
      test_url_(CaptivePortalDetector::kDefaultURL) {
  // The order matters here:
  // |resolve_errors_with_web_service_| must be initialized and |backoff_entry_|
  // created before the call to UpdateEnabledState.
  resolve_errors_with_web_service_.Init(
      prefs::kAlternateErrorPagesEnabled,
      profile_->GetPrefs(),
      base::Bind(&CaptivePortalService::UpdateEnabledState,
                 base::Unretained(this)));
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

  captive_portal_detector_.DetectCaptivePortal(
      test_url_, base::Bind(
          &CaptivePortalService::OnPortalDetectionCompleted,
          base::Unretained(this)));
}

void CaptivePortalService::OnPortalDetectionCompleted(
    const CaptivePortalDetector::Results& results) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(STATE_CHECKING_FOR_PORTAL, state_);
  DCHECK(!TimerRunning());
  DCHECK(enabled_);

  Result result = results.result;
  const base::TimeDelta& retry_after_delta = results.retry_after_delta;
  base::TimeTicks now = GetCurrentTimeTicks();

  // Record histograms.
  UMA_HISTOGRAM_ENUMERATION("CaptivePortal.DetectResult",
                            result,
                            RESULT_COUNT);

  // If this isn't the first captive portal result, record stats.
  if (!last_check_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("CaptivePortal.TimeBetweenChecks",
                             now - last_check_time_);

    if (last_detection_result_ != result) {
      // If the last result was different from the result of the latest test,
      // record histograms about the previous period over which the result was
      // the same.
      RecordRepeatHistograms(last_detection_result_,
                             num_checks_with_same_result_,
                             now - first_check_time_with_same_result_);
    }
  }

  if (last_check_time_.is_null() || result != last_detection_result_) {
    first_check_time_with_same_result_ = now;
    num_checks_with_same_result_ = 1;

    // Reset the backoff entry both to update the default time and clear
    // previous failures.
    ResetBackoffEntry(result);

    backoff_entry_->SetCustomReleaseTime(now + retry_after_delta);
    // The BackoffEntry is not informed of this request, so there's no delay
    // before the next request.  This allows for faster login when a captive
    // portal is first detected.  It can also help when moving between captive
    // portals.
  } else {
    DCHECK_LE(1, num_checks_with_same_result_);
    ++num_checks_with_same_result_;

    // Requests that have the same Result as the last one are considered
    // "failures", to trigger backoff.
    backoff_entry_->SetCustomReleaseTime(now + retry_after_delta);
    backoff_entry_->InformOfRequest(false);
  }

  last_check_time_ = now;

  OnResult(result);
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
  DCHECK(CalledOnValidThread());
  bool enabled_before = enabled_;
  enabled_ = testing_state_ != DISABLED_FOR_TESTING &&
             resolve_errors_with_web_service_.GetValue();

  if (testing_state_ != SKIP_OS_CHECK_FOR_TESTING &&
      HasNativeCaptivePortalDetection()) {
    enabled_ = false;
  }

  if (enabled_before == enabled_)
    return;

  // Clear data used for histograms.
  num_checks_with_same_result_ = 0;
  first_check_time_with_same_result_ = base::TimeTicks();
  last_check_time_ = base::TimeTicks();

  ResetBackoffEntry(last_detection_result_);

  if (state_ == STATE_CHECKING_FOR_PORTAL || state_ == STATE_TIMER_RUNNING) {
    // If a captive portal check was running or pending, cancel check
    // and the timer.
    check_captive_portal_timer_.Stop();
    captive_portal_detector_.Cancel();
    state_ = STATE_IDLE;

    // Since a captive portal request was queued or running, something may be
    // expecting to receive a captive portal result.
    DetectCaptivePortal();
  }
}

base::TimeTicks CaptivePortalService::GetCurrentTimeTicks() const {
  if (time_ticks_for_testing_.is_null())
    return base::TimeTicks::Now();
  else
    return time_ticks_for_testing_;
}

bool CaptivePortalService::DetectionInProgress() const {
  return state_ == STATE_CHECKING_FOR_PORTAL;
}

bool CaptivePortalService::TimerRunning() const {
  return check_captive_portal_timer_.IsRunning();
}

}  // namespace captive_portal
