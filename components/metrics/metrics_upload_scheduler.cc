// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_upload_scheduler.h"

#include <stdint.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {

// This feature moves the upload schedule to a seperate schedule from the
// log rotation schedule.  This may change upload timing slightly, but
// would allow some compartmentalization of uploader logic to allow more
// code reuse between different metrics services.
const base::Feature kUploadSchedulerFeature{"UMAUploadScheduler",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

// When uploading metrics to the server fails, we progressively wait longer and
// longer before sending the next log. This backoff process helps reduce load
// on a server that is having issues.
// The following is the multiplier we use to expand that inter-log duration.
const double kBackoffMultiplier = 1.1;

// The maximum backoff interval in minutes.
const int kMaxBackoffIntervalMinutes = 10;

// Minutes to wait if we are unable to upload due to data usage cap.
const int kOverDataUsageIntervalMinutes = 5;

// Increases the upload interval each time it's called, to handle the case
// where the server is having issues.
base::TimeDelta BackOffUploadInterval(base::TimeDelta interval) {
  DCHECK_GT(kBackoffMultiplier, 1.0);
  interval = base::TimeDelta::FromMicroseconds(static_cast<int64_t>(
      kBackoffMultiplier * interval.InMicroseconds()));

  base::TimeDelta max_interval =
      base::TimeDelta::FromMinutes(kMaxBackoffIntervalMinutes);
  if (interval > max_interval || interval.InSeconds() < 0) {
    interval = max_interval;
  }
  return interval;
}

// Gets a time interval in seconds from a variations parameter.
base::TimeDelta GetTimeParameterSeconds(const std::string& param_name,
                                        int default_seconds) {
  int seconds = base::GetFieldTrialParamByFeatureAsInt(
      kUploadSchedulerFeature, param_name, default_seconds);
  return base::TimeDelta::FromSeconds(seconds);
}

// Time delay after a log is uploaded successfully before attempting another.
// On mobile, keeping the radio on is very expensive, so prefer to keep this
// short and send in bursts.
base::TimeDelta GetUnsentLogsInterval() {
  return GetTimeParameterSeconds("UnsentLogsIntervalSeconds", 3);
}

// Inital time delay after a log uploaded fails before retrying it.
base::TimeDelta GetInitialBackoffInterval() {
  return GetTimeParameterSeconds("InitialBackoffIntervalSeconds", 15);
}

}  // namespace

MetricsUploadScheduler::MetricsUploadScheduler(
    const base::Closure& upload_callback)
    : MetricsScheduler(upload_callback),
      unsent_logs_interval_(GetUnsentLogsInterval()),
      initial_backoff_interval_(GetInitialBackoffInterval()),
      backoff_interval_(initial_backoff_interval_) {}

MetricsUploadScheduler::~MetricsUploadScheduler() {}

void MetricsUploadScheduler::UploadFinished(bool server_is_healthy) {
  // If the server is having issues, back off. Otherwise, reset to default
  // (unless there are more logs to send, in which case the next upload should
  // happen sooner).
  if (!server_is_healthy) {
    TaskDone(backoff_interval_);
    backoff_interval_ = BackOffUploadInterval(backoff_interval_);
  } else {
    backoff_interval_ = initial_backoff_interval_;
    TaskDone(unsent_logs_interval_);
  }
}

void MetricsUploadScheduler::StopAndUploadCancelled() {
  Stop();
  TaskDone(unsent_logs_interval_);
}

void MetricsUploadScheduler::UploadOverDataUsageCap() {
  TaskDone(base::TimeDelta::FromMinutes(kOverDataUsageIntervalMinutes));
}

void MetricsUploadScheduler::LogActualUploadInterval(base::TimeDelta interval) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("UMA.ActualLogUploadInterval",
                              interval.InMinutes(), 1,
                              base::TimeDelta::FromHours(12).InMinutes(), 50);
}

void MetricsUploadScheduler::TriggerTask() {
  if (!last_upload_finish_time_.is_null()) {
    LogActualUploadInterval(base::TimeTicks::Now() - last_upload_finish_time_);
    last_upload_finish_time_ = base::TimeTicks();
  }
  MetricsScheduler::TriggerTask();
}

}  // namespace metrics
