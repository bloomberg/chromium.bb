// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_METRIC_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_METRIC_COLLECTOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/metrics/perf/collection_params.h"
#include "chrome/browser/sessions/session_restore.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// Provides a common interface for metric collectors with custom trigger
// definitions. Extends base::SupportWeakPtr to pass around the "this"
// pointer across threads safely.
class MetricCollector : public base::SupportsWeakPtr<MetricCollector> {
 public:
  MetricCollector();
  explicit MetricCollector(const CollectionParams& collection_params);
  virtual ~MetricCollector();

  // Collector specific initialization.
  virtual void Init() {}

  // Appends collected perf data protobufs to |sampled_profiles|. Clears all the
  // stored profile data. Returns true if it wrote to |sampled_profiles|.
  bool GetSampledProfiles(std::vector<SampledProfile>* sampled_profiles);

  // Turns on profile collection. Resets the timer that's used to schedule
  // collections.
  void OnUserLoggedIn();

  // Turns off profile collection. Does not delete any data that was already
  // collected and stored in |cached_profile_data_|.
  void Deactivate();

  // Called when a suspend finishes. This is a successful suspend followed by
  // a resume.
  void SuspendDone(base::TimeDelta sleep_duration);

  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

 protected:
  const CollectionParams& collection_params() const {
    return collection_params_;
  }

  const base::OneShotTimer& timer() const { return timer_; }

  // Collects perf data after a resume. |sleep_duration| is the duration the
  // system was suspended before resuming. |time_after_resume_ms| is how long
  // ago the system resumed.
  void CollectPerfDataAfterResume(base::TimeDelta sleep_duration,
                                  base::TimeDelta time_after_resume);

  // Collects perf data after a session restore. |time_after_restore| is how
  // long ago the session restore started. |num_tabs_restored| is the total
  // number of tabs being restored.
  void CollectPerfDataAfterSessionRestore(base::TimeDelta time_after_restore,
                                          int num_tabs_restored);

  // Selects a random time in the upcoming profiling interval that begins at
  // |next_profiling_interval_start_|. Schedules |timer_| to invoke
  // DoPeriodicCollection() when that time comes.
  void ScheduleIntervalCollection();

  // Collects profiles on a repeating basis by calling CollectIfNecessary() and
  // reschedules it to be collected again.
  void DoPeriodicCollection();

  // Collects a profile for a given |trigger_event| if necessary.
  void CollectIfNecessary(std::unique_ptr<SampledProfile> sampled_profile);

  // Returns if it's valid and safe for a collector to gather a profile.
  // A collector implementation can override this logic.
  virtual bool ShouldCollect() const;

  // Collector specific logic for collecting a profile.
  virtual void CollectProfile(
      std::unique_ptr<SampledProfile> sampled_profile) = 0;

  // Returns if a collector should upload any profiles at this time. A collector
  // implementation can override this logic.
  virtual bool ShouldUpload() const;

  // Parameters controlling how profiles are collected.
  CollectionParams collection_params_;

  // Vector of SampledProfile protobufs containing perf profiles.
  std::vector<SampledProfile> cached_profile_data_;

  // Record of the last login time.
  base::TimeTicks login_time_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // For scheduling collection of profile data.
  base::OneShotTimer timer_;

  // Record of the start of the upcoming profiling interval.
  base::TimeTicks next_profiling_interval_start_;

  // Tracks the last time a session restore was collected.
  base::TimeTicks last_session_restore_collection_time_;

  DISALLOW_COPY_AND_ASSIGN(MetricCollector);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_METRIC_COLLECTOR_H_
