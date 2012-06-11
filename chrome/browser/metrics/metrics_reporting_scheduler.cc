// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_scheduler.h"

#include "base/compiler_specific.h"
#include "chrome/browser/metrics/metrics_service.h"

using base::TimeDelta;

namespace {

// The delay, in seconds, after startup before sending the first log message.
const int kInitialUploadIntervalSeconds = 60;

// The delay, in seconds, between uploading when there are queued logs from
// previous sessions to send.
const int kUnsentLogsIntervalSeconds = 15;

// Standard interval between log uploads, in seconds.
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.

// When uploading metrics to the server fails, we progressively wait longer and
// longer before sending the next log. This backoff process helps reduce load
// on a server that is having issues.
// The following is the multiplier we use to expand that inter-log duration.
const double kBackoffMultiplier = 1.1;

// The maximum backoff multiplier.
const int kMaxBackoffMultiplier = 10;

}  // anonymous namespace

MetricsReportingScheduler::MetricsReportingScheduler(
    const base::Closure& upload_callback)
    : upload_callback_(upload_callback),
      upload_interval_(TimeDelta::FromSeconds(kInitialUploadIntervalSeconds)),
      running_(false),
      callback_pending_(false) {
}

MetricsReportingScheduler::~MetricsReportingScheduler() {}

void MetricsReportingScheduler::Start() {
  running_ = true;
  ScheduleNextUpload();
}

void MetricsReportingScheduler::Stop() {
  running_ = false;
  if (upload_timer_.IsRunning())
    upload_timer_.Stop();
}

void MetricsReportingScheduler::UploadFinished(bool server_is_healthy,
                                               bool more_logs_remaining) {
  DCHECK(callback_pending_);
  callback_pending_ = false;
  // If the server is having issues, back off. Otherwise, reset to default
  // (unless there are more logs to send, in which case the next upload should
  // happen sooner).
  if (!server_is_healthy) {
    BackOffUploadInterval();
  } else if (more_logs_remaining) {
    upload_interval_ = TimeDelta::FromSeconds(kUnsentLogsIntervalSeconds);
  } else {
    upload_interval_ = TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
  }

  if (running_)
    ScheduleNextUpload();
}

void MetricsReportingScheduler::UploadCancelled() {
  DCHECK(callback_pending_);
  callback_pending_ = false;
  if (running_)
    ScheduleNextUpload();
}

void MetricsReportingScheduler::TriggerUpload() {
  callback_pending_ = true;
  upload_callback_.Run();
}

void MetricsReportingScheduler::ScheduleNextUpload() {
  DCHECK(running_);
  if (upload_timer_.IsRunning() || callback_pending_)
    return;

  upload_timer_.Start(FROM_HERE, upload_interval_, this,
                      &MetricsReportingScheduler::TriggerUpload);
}

void MetricsReportingScheduler::BackOffUploadInterval() {
  DCHECK_GT(kBackoffMultiplier, 1.0);
  upload_interval_ = TimeDelta::FromMicroseconds(
      static_cast<int64>(kBackoffMultiplier *
                         upload_interval_.InMicroseconds()));

  TimeDelta max_interval = kMaxBackoffMultiplier *
      TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
  if (upload_interval_ > max_interval || upload_interval_.InSeconds() < 0) {
    upload_interval_ = max_interval;
  }
}
