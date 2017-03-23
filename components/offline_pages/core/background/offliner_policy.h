// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_H_

#include "base/sys_info.h"

namespace {
// The max number of started tries is to guard against pages that make the
// prerenderer crash.  It should be greater than or equal to the max number of
// completed tries.
const int kMaxStartedTries = 5;
// The number of max completed tries is based on Gin2G-poor testing showing that
// we often need about 4 tries with a 2 minute window, or 3 retries with a 3
// minute window. Also, we count one try now for foreground/disabled requests.
const int kMaxCompletedTries = 3;
// By the time we get to a week, the user has forgotten asking for a page.
const int kRequestExpirationTimeInSeconds = 60 * 60 * 24 * 7;

// Scheduled background processing time limits for doze mode, which requires
// Android version >= 6.0 (API level >= 23). Otherwise the scheduled background
// processing time should be the same as immediate loading (4 min 50 secs), it's
// capped by the Prerenderer's 5 minute timeout.
const int kDozeModeBackgroundServiceWindowSeconds = 60 * 3;
const int kDefaultBackgroundProcessingTimeBudgetSeconds =
    kDozeModeBackgroundServiceWindowSeconds - 10;
const int kSinglePageTimeLimitWhenBackgroundScheduledSeconds =
    kDozeModeBackgroundServiceWindowSeconds - 10;

const int kNonDozeModeBackgroundServiceWindowSeconds = 60 * 5;
const int kNonDozeDefaultBackgroundProcessingTimeBudgetSeconds =
    kNonDozeModeBackgroundServiceWindowSeconds - 10;
const int kNonDozeSinglePageTimeLimitWhenBackgroundScheduledSeconds =
    kNonDozeModeBackgroundServiceWindowSeconds - 10;

// Immediate processing time limits.  Note: experiments on GIN-2g-poor show many
// page requests took 3 or 4 attempts in background scheduled mode with timeout
// of 2 minutes. So for immediate processing mode, give page requests just under
// 5 minutes, which is the timeout limit for the prerender itself. Then budget
// up to 3 of those requests in processing window.
const int kSinglePageTimeLimitForImmediateLoadSeconds = 60 * 4 + 50;
const int kImmediateLoadProcessingTimeBudgetSeconds =
    kSinglePageTimeLimitForImmediateLoadSeconds * 5;
}  // namespace

namespace offline_pages {

// Policy for the Background Offlining system.  Some policy will belong to the
// RequestCoordinator, some to the RequestQueue, and some to the Offliner.
class OfflinerPolicy {
 public:
  OfflinerPolicy()
      : prefer_untried_requests_(true),
        prefer_earlier_requests_(true),
        retry_count_is_more_important_than_recency_(true),
        max_started_tries_(kMaxStartedTries),
        max_completed_tries_(kMaxCompletedTries) {
    int32_t os_major_version = 0;
    int32_t os_minor_version = 0;
    int32_t os_bugfix_version = 0;
    base::SysInfo::OperatingSystemVersionNumbers(
        &os_major_version, &os_minor_version, &os_bugfix_version);
    if (os_major_version < 6)
      has_doze_mode_ = false;
    else
      has_doze_mode_ = true;
  }

  // Constructor for unit tests.
  OfflinerPolicy(bool prefer_untried,
                 bool prefer_earlier,
                 bool prefer_retry_count,
                 int max_started_tries,
                 int max_completed_tries)
      : prefer_untried_requests_(prefer_untried),
        prefer_earlier_requests_(prefer_earlier),
        retry_count_is_more_important_than_recency_(prefer_retry_count),
        max_started_tries_(max_started_tries),
        max_completed_tries_(max_completed_tries) {}

  // TODO(petewil): Numbers here are chosen arbitrarily, do the proper studies
  // to get good policy numbers. Eventually this should get data from a finch
  // experiment.

  // Returns true if we should prefer retrying lesser tried requests.
  bool ShouldPreferUntriedRequests() const { return prefer_untried_requests_; }

  // Returns true if we should prefer older requests of equal number of tries.
  bool ShouldPreferEarlierRequests() const { return prefer_earlier_requests_; }

  // Returns true if retry count is considered more important than recency in
  // picking which request to try next.
  bool RetryCountIsMoreImportantThanRecency() const {
    return retry_count_is_more_important_than_recency_;
  }

  // The max number of times we will start a request.  Not all started attempts
  // will complete.  This may be caused by prerenderer issues or chromium being
  // swapped out of memory.
  int GetMaxStartedTries() const { return max_started_tries_; }

  // The max number of times we will retry a request when the attempt
  // completed, but failed.
  int GetMaxCompletedTries() const { return max_completed_tries_; }

  bool PowerRequired(bool user_requested) const { return (!user_requested); }

  bool UnmeteredNetworkRequired(bool user_requested) const {
    return !(user_requested);
  }

  int BatteryPercentageRequired(bool user_requested) const {
    if (user_requested)
      return 0;
    // This is so low because we require the device to be plugged in and
    // charging.  If we decide to allow non-user requested pages when not
    // plugged in, we should raise this somewhat higher.
    return 25;
  }

  // How many seconds to keep trying new pages for, before we give up, and
  // return to the scheduler.
  // TODO(dougarnett): Consider parameterizing these time limit/budget
  // calls with processing mode.
  int GetProcessingTimeBudgetWhenBackgroundScheduledInSeconds() const {
    if (has_doze_mode_)
      return kDefaultBackgroundProcessingTimeBudgetSeconds;
    return kNonDozeDefaultBackgroundProcessingTimeBudgetSeconds;
  }

  // How many seconds to keep trying new pages for, before we give up, when
  // processing started immediately (without scheduler).
  int GetProcessingTimeBudgetForImmediateLoadInSeconds() const {
    return kImmediateLoadProcessingTimeBudgetSeconds;
  }

  // How long do we allow a page to load before giving up on it when
  // background loading was scheduled.
  int GetSinglePageTimeLimitWhenBackgroundScheduledInSeconds() const {
    if (has_doze_mode_)
      return kSinglePageTimeLimitWhenBackgroundScheduledSeconds;
    return kNonDozeSinglePageTimeLimitWhenBackgroundScheduledSeconds;
  }

  // How long do we allow a page to load before giving up on it when
  // immediately background loading.
  int GetSinglePageTimeLimitForImmediateLoadInSeconds() const {
    return kSinglePageTimeLimitForImmediateLoadSeconds;
  }

  // How long we allow requests to remain in the system before giving up.
  int GetRequestExpirationTimeInSeconds() const {
    return kRequestExpirationTimeInSeconds;
  }

 private:
  bool prefer_untried_requests_;
  bool prefer_earlier_requests_;
  bool retry_count_is_more_important_than_recency_;
  bool has_doze_mode_;
  int max_started_tries_;
  int max_completed_tries_;
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_OFFLINER_POLICY_H_
