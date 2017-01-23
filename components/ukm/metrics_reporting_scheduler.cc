// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/metrics_reporting_scheduler.h"

#include "base/metrics/histogram_macros.h"

namespace ukm {

MetricsReportingScheduler::MetricsReportingScheduler(
    const base::Closure& upload_callback,
    const base::Callback<base::TimeDelta(void)>& upload_interval_callback)
    : metrics::MetricsReportingScheduler(upload_callback,
                                         upload_interval_callback) {}

MetricsReportingScheduler::~MetricsReportingScheduler() = default;

void MetricsReportingScheduler::LogMetricsInitSequence(InitSequence sequence) {
  UMA_HISTOGRAM_ENUMERATION("UKM.InitSequence", sequence,
                            INIT_SEQUENCE_ENUM_SIZE);
}

void MetricsReportingScheduler::LogActualUploadInterval(
    base::TimeDelta interval) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("UKM.ActualLogUploadInterval",
                              interval.InMinutes(), 1,
                              base::TimeDelta::FromHours(12).InMinutes(), 50);
}

}  // namespace ukm
