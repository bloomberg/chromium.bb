// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_service_base.h"

#include <cstdlib>

#include "chrome/common/metrics/metrics_log_base.h"

using base::Histogram;

MetricsServiceBase::MetricsServiceBase()
    : ALLOW_THIS_IN_INITIALIZER_LIST(histogram_snapshot_manager_(this)) {
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
  histogram_snapshot_manager_.PrepareDeltas(base::Histogram::kNoFlags, true);
}

void MetricsServiceBase::RecordDelta(
    const base::HistogramBase& histogram,
    const base::HistogramSamples& snapshot) {
  log_manager_.current_log()->RecordHistogramDelta(histogram.histogram_name(),
                                                   snapshot);
}

void MetricsServiceBase::InconsistencyDetected(
    base::HistogramBase::Inconsistency problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowser",
                            problem, base::HistogramBase::NEVER_EXCEEDED_VALUE);
}

void MetricsServiceBase::UniqueInconsistencyDetected(
    base::HistogramBase::Inconsistency problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowserUnique",
                            problem, base::HistogramBase::NEVER_EXCEEDED_VALUE);
}

void MetricsServiceBase::InconsistencyDetectedInLoggedCount(int amount) {
  UMA_HISTOGRAM_COUNTS("Histogram.InconsistentSnapshotBrowser",
                       std::abs(amount));
}
