// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_classification.h"

#include "base/build_time.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "components/network_time/network_time_tracker.h"
#include "net/cert/x509_certificate.h"

using base::Time;
using base::TimeTicks;
using base::TimeDelta;

namespace {

// Events for UMA. Do not reorder or change!
enum SSLInterstitialCause {
  CLOCK_PAST,
  CLOCK_FUTURE,
  UNUSED_INTERSTITIAL_CAUSE_ENTRY,
};

void RecordSSLInterstitialCause(bool overridable, SSLInterstitialCause event) {
  if (overridable) {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.overridable", event,
                              UNUSED_INTERSTITIAL_CAUSE_ENTRY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.nonoverridable", event,
                              UNUSED_INTERSTITIAL_CAUSE_ENTRY);
  }
}

} // namespace

SSLErrorClassification::SSLErrorClassification(
    base::Time current_time,
    const net::X509Certificate& cert)
  : current_time_(current_time),
    cert_(cert) { }

SSLErrorClassification::~SSLErrorClassification() { }

float SSLErrorClassification::InvalidDateSeverityScore() const {
  // Client-side characterisitics. Check whether the system's clock is wrong or
  // not and whether the user has encountered this error before or not.
  float severity_date_score = 0.0f;

  static const float kClientWeight = 0.5f;
  static const float kSystemClockWeight = 0.75f;
  static const float kSystemClockWrongWeight = 0.1f;
  static const float kSystemClockRightWeight = 1.0f;

  static const float kServerWeight = 0.5f;
  static const float kCertificateExpiredWeight = 0.3f;
  static const float kNotYetValidWeight = 0.2f;

  if (IsUserClockInThePast(current_time_)  ||
      IsUserClockInTheFuture(current_time_)) {
    severity_date_score = kClientWeight * kSystemClockWeight *
        kSystemClockWrongWeight;
  } else {
    severity_date_score = kClientWeight * kSystemClockWeight *
        kSystemClockRightWeight;
  }
  // TODO(radhikabhar): (crbug.com/393262) Check website settings.

  // Server-side characteristics. Check whether the certificate has expired or
  // is not yet valid. If the certificate has expired then factor the time which
  // has passed since expiry.
  if (cert_.HasExpired()) {
    severity_date_score += kServerWeight * kCertificateExpiredWeight *
        CalculateScoreTimePassedSinceExpiry();
  }
  if (current_time_ < cert_.valid_start())
    severity_date_score += kServerWeight * kNotYetValidWeight;
  return severity_date_score;
}

base::TimeDelta SSLErrorClassification::TimePassedSinceExpiry() const {
  base::TimeDelta delta = current_time_ - cert_.valid_expiry();
  return delta;
}

float SSLErrorClassification::CalculateScoreTimePassedSinceExpiry() const {
  base::TimeDelta delta = TimePassedSinceExpiry();
  int64 time_passed = delta.InDays();
  const int64 kHighThreshold = 7;
  const int64 kLowThreshold = 4;
  static const float kHighThresholdWeight = 0.4f;
  static const float kMediumThresholdWeight = 0.3f;
  static const float kLowThresholdWeight = 0.2f;
  if (time_passed >= kHighThreshold)
    return kHighThresholdWeight;
  else if (time_passed >= kLowThreshold)
    return kMediumThresholdWeight;
  else
    return kLowThresholdWeight;
}

bool SSLErrorClassification::IsUserClockInThePast(base::Time time_now) {
  base::Time build_time = base::GetBuildTime();
  if (time_now < build_time - base::TimeDelta::FromDays(2))
    return true;
  return false;
}

bool SSLErrorClassification::IsUserClockInTheFuture(base::Time time_now) {
  base::Time build_time = base::GetBuildTime();
  if (time_now > build_time + base::TimeDelta::FromDays(365))
    return true;
  return false;
}

void SSLErrorClassification::RecordUMAStatistics(bool overridable) {
  if (IsUserClockInThePast(base::Time::NowFromSystemTime()))
    RecordSSLInterstitialCause(overridable, CLOCK_PAST);
  if (IsUserClockInTheFuture(base::Time::NowFromSystemTime()))
    RecordSSLInterstitialCause(overridable, CLOCK_FUTURE);
}
