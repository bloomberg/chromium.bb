// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_

namespace {
const int kMaxRetries = 2;
const int kBackgroundTimeBudgetSeconds = 170;
const int kSinglePageTimeBudgetSeconds = 120;
const int kMinimumBatteryPercentageForNonUserRequestOfflining = 50;
}  // namespace

namespace offline_pages {

// Policy for the Background Offlining system.  Some policy will belong to the
// RequestCoordinator, some to the RequestQueue, and some to the Offliner.
class OfflinerPolicy {
 public:
  OfflinerPolicy(){};

  // TODO(petewil): Numbers here are chosen arbitrarily, do the proper studies
  // to get good policy numbers.

  // TODO(petewil): Eventually this should get data from a finch experiment.

  // Returns true if we should prefer retrying lesser tried requests.
  bool ShouldPreferUntriedRequests() { return false; }

  // Returns true if we should prefer older requests of equal number of tries.
  bool ShouldPreferEarlierRequests() { return true; }

  // Returns true if retry count is considered more important than recency in
  // picking which request to try next.
  bool RetryCountIsMoreImportantThanRecency() { return true; }

  // The max number of times we will retry a request.
  int GetMaxRetries() { return kMaxRetries; }

  // How many seconds to keep trying new pages for, before we give up,  and
  // return to the scheduler.
  int GetBackgroundProcessingTimeBudgetSeconds() {
    return kBackgroundTimeBudgetSeconds;
  }

  // How long do we allow a page to load before giving up on it
  int GetSinglePageTimeBudgetInSeconds() {
    return kSinglePageTimeBudgetSeconds;
  }

  // How much battery must we have before fetching a page not explicitly
  // requested by the user?
  int GetMinimumBatteryPercentageForNonUserRequestOfflining() {
    return kMinimumBatteryPercentageForNonUserRequestOfflining;
  }
};
}

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_POLICY_H_
