// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/collection_params.h"

#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace metrics {

namespace {

// Returns a TimeDelta profile duration based on the current chrome channel.
base::TimeDelta ProfileDuration() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      return base::TimeDelta::FromSeconds(4);
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
    default:
      return base::TimeDelta::FromSeconds(2);
  }
}

// Returns a TimeDelta interval duration for periodic collection based on the
// current chrome channel.
base::TimeDelta PeriodicCollectionInterval() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      return base::TimeDelta::FromMinutes(90);
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
    default:
      return base::TimeDelta::FromMinutes(180);
  }
}

}  // namespace

// Defines default collection parameters.
CollectionParams::CollectionParams()
    : CollectionParams(
          ProfileDuration() /* collection_duration */,
          PeriodicCollectionInterval() /* periodic_interval */,
          CollectionParams::TriggerParams(/* resume_from_suspend */
                                          10 /* sampling_factor */,
                                          base::TimeDelta::FromSeconds(
                                              5)) /* max_collection_delay */,
          CollectionParams::TriggerParams(/* restore_session */
                                          10 /* sampling_factor */,
                                          base::TimeDelta::FromSeconds(
                                              10)) /* max_collection_delay */) {
}

CollectionParams::CollectionParams(base::TimeDelta collection_duration,
                                   base::TimeDelta periodic_interval,
                                   TriggerParams resume_from_suspend,
                                   TriggerParams restore_session)
    : collection_duration_(collection_duration),
      periodic_interval_(periodic_interval),
      resume_from_suspend_(resume_from_suspend),
      restore_session_(restore_session) {}

CollectionParams::TriggerParams::TriggerParams(
    int64_t sampling_factor,
    base::TimeDelta max_collection_delay)
    : sampling_factor_(sampling_factor),
      max_collection_delay_(max_collection_delay) {}

}  // namespace metrics
