// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_REPORTING_SCHEDULER_H_
#define CHROME_BROWSER_METRICS_METRICS_REPORTING_SCHEDULER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"

// Scheduler task to drive a MetricsService object's uploading.
class MetricsReportingScheduler {
 public:
  explicit MetricsReportingScheduler(const base::Closure& upload_callback);
  ~MetricsReportingScheduler();

  // Starts scheduling uploads. This in a no-op if the scheduler is already
  // running, so it is safe to call more than once.
  void Start();

  // Stops scheduling uploads.
  void Stop();

  // Callback from MetricsService when a triggered upload finishes.
  void UploadFinished(bool server_is_healthy, bool more_logs_remaining);

  // Callback from MetricsService when a triggered upload is cancelled by the
  // MetricsService.
  void UploadCancelled();

 private:
  // Timer callback indicating it's time for the MetricsService to upload
  // metrics.
  void TriggerUpload();

  // Schedules a future call to TriggerUpload if one isn't already pending.
  void ScheduleNextCallback();

  // Increases the upload interval each time it's called, to handle the case
  // where the server is having issues.
  void BackOffUploadInterval();

  // The MetricsService method to call when uploading should happen.
  base::Closure upload_callback_;

  base::WeakPtrFactory<MetricsReportingScheduler> weak_ptr_factory_;

  // The interval between being told an upload is done and starting the next
  // upload.
  base::TimeDelta upload_interval_;

  // Indicates that the scheduler is running (i.e., that Start has been called
  // more recently than Stop).
  bool running_;

  // Indicates that a timer for triggering the next upload has already been
  // started.
  bool timer_pending_;

  // Indicates that the last triggered upload hasn't resolved yet.
  bool callback_pending_;

  DISALLOW_COPY_AND_ASSIGN(MetricsReportingScheduler);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_REPORTING_SCHEDULER_H_
