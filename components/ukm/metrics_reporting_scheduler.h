// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_METRICS_REPORTING_SCHEDULER_H_
#define COMPONENTS_UKM_METRICS_REPORTING_SCHEDULER_H_

#include "base/time/time.h"
#include "components/metrics/metrics_reporting_scheduler.h"

namespace ukm {

// Scheduler task to drive a MetricsService object's uploading.
class MetricsReportingScheduler : public metrics::MetricsReportingScheduler {
 public:
  // Creates MetricsServiceScheduler object with the given |upload_callback|
  // callback to call when uploading should happen and
  // |upload_interval_callback| to determine the interval between uploads
  // in steady state.
  MetricsReportingScheduler(
      const base::Closure& upload_callback,
      const base::Callback<base::TimeDelta(void)>& upload_interval_callback);
  ~MetricsReportingScheduler() override;

 private:
  // Record the init sequence order histogram.
  void LogMetricsInitSequence(InitSequence sequence) override;

  // Record the upload interval time.
  void LogActualUploadInterval(base::TimeDelta interval) override;

  DISALLOW_COPY_AND_ASSIGN(MetricsReportingScheduler);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_METRICS_REPORTING_SCHEDULER_H_
