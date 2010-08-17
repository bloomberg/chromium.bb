// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/request_throttler/request_throttler_entry.h"

#include <cmath>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "net/request_throttler/request_throttler_header_interface.h"

const int RequestThrottlerEntry::kAdditionalConstantMs = 100;
const int RequestThrottlerEntry::kEntryLifetimeSec = 120;
const double RequestThrottlerEntry::kJitterFactor = 0.4;
const int RequestThrottlerEntry::kInitialBackoffMs = 700;
const int RequestThrottlerEntry::kMaximumBackoffMs = 24 * 60 * 60 * 1000;
const double RequestThrottlerEntry::kMultiplyFactor = 2.0;
const char RequestThrottlerEntry::kRetryHeaderName[] = "X-Retry-After";

RequestThrottlerEntry::RequestThrottlerEntry()
    : release_time_(base::TimeTicks::Now()),
      num_times_delayed_(0),
      is_managed_(false) {
  SaveState();
}

RequestThrottlerEntry::~RequestThrottlerEntry() {
}

bool RequestThrottlerEntry::IsRequestAllowed() const {
  return (release_time_ <= GetTimeNow());
}

void RequestThrottlerEntry::UpdateWithResponse(
    const RequestThrottlerHeaderInterface* response) {
  SaveState();
  if (response->GetResponseCode() >= 500) {
    num_times_delayed_++;
    release_time_ = std::max(CalculateReleaseTime(), release_time_);
    is_managed_ = true;
  } else {
    // We slowly decay the number of times delayed instead of resetting it to 0
    // in order to stay stable if we received lots of requests with
    // malformed bodies at the same time.
    if (num_times_delayed_ > 0)
      num_times_delayed_--;
    is_managed_ = false;
    // The reason why we are not just cutting release_time to GetTimeNow() is
    // on the one hand, it would unset delay put by our custom retry-after
    // header and on the other we would like to push every request up to our
    // "horizon" when dealing with multiple in-flight request. Ex: If we send
    // three request and we receive 2 failures and 1 success. The success that
    // follows those failures will not reset release time further request will
    // then need to wait the delay caused by the 2 failures.
    release_time_ = std::max(GetTimeNow(), release_time_);
    if (response->GetNormalizedValue(kRetryHeaderName) != std::string())
      HandleCustomRetryAfter(response->GetNormalizedValue(kRetryHeaderName));
  }
}

bool RequestThrottlerEntry::IsEntryOutdated() const {
  int64 unused_since_ms = (GetTimeNow() - release_time_).InMilliseconds();
  int64 lifespan_ms = kEntryLifetimeSec * 1000;

  // Release time is further than now, we are managing it.
  if (unused_since_ms < 0)
    return false;

  // There are two cases. First one, when the entry is currently being managed
  // and should not be collected unless it is older than the maximum allowed
  // back-off. The other one, when the entry is outdated, unmanaged and should
  // be collected.
  if (is_managed_)
    return unused_since_ms > kMaximumBackoffMs;

  return unused_since_ms > lifespan_ms;
}

void RequestThrottlerEntry::ReceivedContentWasMalformed() {
  // We should never revert to less back-off or else an attacker could put a
  // malformed body in cache and replay it to decrease delay.
  num_times_delayed_ =
      std::max(old_values_.number_of_failed_requests, num_times_delayed_);
  num_times_delayed_++;
  is_managed_ = true;
  release_time_ = std::max(CalculateReleaseTime(),
      std::max(old_values_.release_time, release_time_));
}

base::TimeTicks RequestThrottlerEntry::CalculateReleaseTime() {
  double delay = kInitialBackoffMs;
  delay *= pow(kMultiplyFactor, num_times_delayed_);
  delay += kAdditionalConstantMs;
  delay -= base::RandDouble() * kJitterFactor * delay;

  // Ensure that we do not exceed maximum delay
  int64 delay_int = static_cast<int64>(delay + 0.5);
  delay_int = std::min(delay_int, static_cast<int64>(kMaximumBackoffMs));

  return GetTimeNow() + base::TimeDelta::FromMilliseconds(delay_int);
}

base::TimeTicks RequestThrottlerEntry::GetTimeNow() const {
  return base::TimeTicks::Now();
}

void RequestThrottlerEntry::HandleCustomRetryAfter(
    const std::string& header_value) {
  // Input parameter is the number of seconds to wait in a floating point value.
  double time_in_sec = 0;
  bool conversion_is_ok = base::StringToDouble(header_value, &time_in_sec);

  // Conversion of custom retry-after header value failed.
  if (!conversion_is_ok)
    return;

  // We must use an int value later so we transform this in milliseconds.
  int64 value_ms = static_cast<int64>(0.5 + time_in_sec * 1000);

  if (kMaximumBackoffMs < value_ms || value_ms < 0)
    return;

  release_time_ = std::max(
      (GetTimeNow() + base::TimeDelta::FromMilliseconds(value_ms)),
      release_time_);
}

void RequestThrottlerEntry::SaveState() {
  old_values_.release_time = release_time_;
  old_values_.number_of_failed_requests = num_times_delayed_;
}
