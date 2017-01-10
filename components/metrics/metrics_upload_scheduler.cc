// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_upload_scheduler.h"

#include <stdint.h>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/metrics/metrics_scheduler.h"

namespace metrics {

namespace {

// The delay, in seconds, between uploading when there are queued logs from
// previous sessions to send. Sending in a burst is better on a mobile device,
// since keeping the radio on is very expensive, so prefer to keep this short.
const int kUnsentLogsIntervalSeconds = 3;

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

}  // namespace

MetricsUploadScheduler::MetricsUploadScheduler(
    const base::Closure& upload_callback)
    : MetricsScheduler(upload_callback),
      upload_interval_(
          base::TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds)) {}

MetricsUploadScheduler::~MetricsUploadScheduler() {}

void MetricsUploadScheduler::UploadFinished(bool server_is_healthy) {
  // If the server is having issues, back off. Otherwise, reset to default
  // (unless there are more logs to send, in which case the next upload should
  // happen sooner).
  if (!server_is_healthy) {
    upload_interval_ = BackOffUploadInterval(upload_interval_);
  } else {
    upload_interval_ = base::TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds);
  }
  TaskDone(upload_interval_);
}

void MetricsUploadScheduler::UploadCancelled() {
  TaskDone(upload_interval_);
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
