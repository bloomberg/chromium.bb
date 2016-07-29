// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_

namespace {
const int kMaxTries = 1;
const int kBackgroundProcessingTimeBudgetSeconds = 170;
const int kSinglePageTimeLimitSeconds = 120;
const int kMinimumBatteryPercentageForNonUserRequestOfflining = 50;
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
        retry_count_is_more_important_than_recency_(false) {}

  OfflinerPolicy(bool prefer_untried,
                 bool prefer_earlier,
                 bool prefer_retry_count)
      : prefer_untried_requests_(prefer_untried),
        prefer_earlier_requests_(prefer_earlier),
        retry_count_is_more_important_than_recency_(prefer_retry_count) {}

  // TODO(petewil): Numbers here are chosen arbitrarily, do the proper studies
  // to get good policy numbers.

  // TODO(petewil): Eventually this should get data from a finch experiment.

  // Returns true if we should prefer retrying lesser tried requests.
  bool ShouldPreferUntriedRequests() const { return prefer_untried_requests_; }

  // Returns true if we should prefer older requests of equal number of tries.
  bool ShouldPreferEarlierRequests() const { return prefer_earlier_requests_; }

  // Returns true if retry count is considered more important than recency in
  // picking which request to try next.
  bool RetryCountIsMoreImportantThanRecency() const {
    return retry_count_is_more_important_than_recency_;
  }

  // The max number of times we will retry a request.
  int GetMaxTries() const { return kMaxTries; }

  bool PowerRequiredForUserRequestedPage() const { return false; }

  bool PowerRequiredForNonUserRequestedPage() const { return true; }

  bool UnmeteredNetworkRequiredForUserRequestedPage() const { return false; }

  bool UnmeteredNetworkRequiredForNonUserRequestedPage() const { return true; }

  int BatteryPercentageRequiredForUserRequestedPage() const { return 0; }

  // This is so low because we require the device to be plugged in and charging.
  // If we decide to allow non-user requested pages when not plugged in, we
  // should raise this somewhat higher.
  int BatteryPercentageRequiredForNonUserRequestedPage() const { return 25; }

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

  // How much battery must we have before fetching a page not explicitly
  // requested by the user?
  int GetMinimumBatteryPercentageForNonUserRequestOfflining() const {
    return kMinimumBatteryPercentageForNonUserRequestOfflining;
  }

 private:
  bool prefer_untried_requests_;
  bool prefer_earlier_requests_;
  bool retry_count_is_more_important_than_recency_;
};
}

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
