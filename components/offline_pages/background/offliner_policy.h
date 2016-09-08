// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_

namespace {
const int kMaxStartedTries = 4;
const int kMaxCompletedTries = 1;
const int kBackgroundProcessingTimeBudgetSeconds = 170;
const int kSinglePageTimeLimitSeconds = 120;
const int kRequestExpirationTimeInSeconds = 60 * 60 * 24 * 7;
}  // namespace

namespace offline_pages {

// Policy for the Background Offlining system.  Some policy will belong to the
// RequestCoordinator, some to the RequestQueue, and some to the Offliner.
class OfflinerPolicy {
 public:
  OfflinerPolicy()
      : prefer_untried_requests_(false),
        prefer_earlier_requests_(true),
        retry_count_is_more_important_than_recency_(false),
        max_started_tries_(kMaxStartedTries),
        max_completed_tries_(kMaxCompletedTries) {}

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

  bool PowerRequired(bool user_requested) const {
    return (!user_requested);
  }

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

  // How many seconds to keep trying new pages for, before we give up,  and
  // return to the scheduler.
  int GetBackgroundProcessingTimeBudgetSeconds() const {
    return kBackgroundProcessingTimeBudgetSeconds;
  }

  // How long do we allow a page to load before giving up on it
  int GetSinglePageTimeLimitInSeconds() const {
    return kSinglePageTimeLimitSeconds;
  }

  // How long we allow requests to remain in the system before giving up.
  int GetRequestExpirationTimeInSeconds() const {
    return kRequestExpirationTimeInSeconds;
  }

 private:
  bool prefer_untried_requests_;
  bool prefer_earlier_requests_;
  bool retry_count_is_more_important_than_recency_;
  int max_started_tries_;
  int max_completed_tries_;
};
}

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
