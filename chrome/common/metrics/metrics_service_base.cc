// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_service_base.h"

#include <cstdlib>

#include "chrome/common/metrics/metrics_log_base.h"

using base::Histogram;

MetricsServiceBase::MetricsServiceBase() {
}

MetricsServiceBase::~MetricsServiceBase() {
}

// static
const char MetricsServiceBase::kServerUrlXml[] =
    "https://clients4.google.com/firefox/metrics/collect";
const char MetricsServiceBase::kServerUrlProto[] =
    "https://clients4.google.com/uma/v2";

// static
const char MetricsServiceBase::kMimeTypeXml[] =
    "application/vnd.mozilla.metrics.bz2";
// static
const char MetricsServiceBase::kMimeTypeProto[] =
    "application/vnd.chrome.uma";

void MetricsServiceBase::RecordCurrentHistograms() {
  DCHECK(log_manager_.current_log());
  TransmitAllHistograms(base::Histogram::kNoFlags, true);
}

void MetricsServiceBase::TransmitHistogramDelta(
    const base::Histogram& histogram,
    const base::Histogram::SampleSet& snapshot) {
  log_manager_.current_log()->RecordHistogramDelta(histogram, snapshot);
}

void MetricsServiceBase::InconsistencyDetected(int problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowser",
                            problem, Histogram::NEVER_EXCEEDED_VALUE);
}

void MetricsServiceBase::UniqueInconsistencyDetected(int problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowserUnique",
                            problem, Histogram::NEVER_EXCEEDED_VALUE);
}

void MetricsServiceBase::SnapshotProblemResolved(int amount) {
  UMA_HISTOGRAM_COUNTS("Histogram.InconsistentSnapshotBrowser",
                       std::abs(amount));
}
