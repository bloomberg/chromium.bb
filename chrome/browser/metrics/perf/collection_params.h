// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_COLLECTION_PARAMS_H_
#define CHROME_BROWSER_METRICS_PERF_COLLECTION_PARAMS_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace metrics {

// Defines collection parameters for metric collectors. Each collection trigger
// has its own set of parameters.
class CollectionParams {
 public:
  class TriggerParams {
   public:
    TriggerParams(int64_t sampling_factor,
                  base::TimeDelta max_collection_delay);

    int64_t sampling_factor() const { return sampling_factor_; }
    void set_sampling_factor(int64_t factor) { sampling_factor_ = factor; }

    base::TimeDelta max_collection_delay() const {
      return max_collection_delay_;
    }
    void set_max_collection_delay(base::TimeDelta delay) {
      max_collection_delay_ = delay;
    }

   private:
    TriggerParams() = delete;

    // Limit the number of profiles collected.
    int64_t sampling_factor_;

    // Add a random delay before collecting after the trigger.
    // The delay should be randomly selected between 0 and this value.
    base::TimeDelta max_collection_delay_;
  };

  CollectionParams();

  CollectionParams(base::TimeDelta collection_duration,
                   base::TimeDelta periodic_interval,
                   TriggerParams resume_from_suspend,
                   TriggerParams restore_session);

  base::TimeDelta collection_duration() const { return collection_duration_; }
  void set_collection_duration(base::TimeDelta duration) {
    collection_duration_ = duration;
  }

  base::TimeDelta periodic_interval() const { return periodic_interval_; }
  void set_periodic_interval(base::TimeDelta interval) {
    periodic_interval_ = interval;
  }

  const TriggerParams& resume_from_suspend() const {
    return resume_from_suspend_;
  }
  TriggerParams* mutable_resume_from_suspend() { return &resume_from_suspend_; }
  const TriggerParams& restore_session() const { return restore_session_; }
  TriggerParams* mutable_restore_session() { return &restore_session_; }

 private:
  // Time perf is run for.
  base::TimeDelta collection_duration_;

  // For PERIODIC_COLLECTION, partition time since login into successive
  // intervals of this duration. In each interval, a random time is picked to
  // collect a profile.
  base::TimeDelta periodic_interval_;

  // Parameters for RESUME_FROM_SUSPEND and RESTORE_SESSION collections:
  TriggerParams resume_from_suspend_;
  TriggerParams restore_session_;

  DISALLOW_COPY_AND_ASSIGN(CollectionParams);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_COLLECTION_PARAMS_H_
