// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_metrics.h"

#include "base/metrics/histogram.h"

AutoFillMetrics::AutoFillMetrics() {
}

AutoFillMetrics::~AutoFillMetrics() {
}

void AutoFillMetrics::Log(ServerQueryMetric metric) const {
  DCHECK(metric < NUM_SERVER_QUERY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("AutoFill.ServerQueryResponse", metric,
                            NUM_SERVER_QUERY_METRICS);
}

void AutoFillMetrics::Log(QualityMetric metric) const {
  DCHECK(metric < NUM_QUALITY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("AutoFill.Quality", metric,
                            NUM_QUALITY_METRICS);
}
