// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_metrics.h"

#include "base/metrics/histogram.h"

namespace autofill_metrics {

void LogServerQueryMetric(ServerQueryMetricType type) {
  DCHECK(type < NUM_SERVER_QUERY_METRICS);

  UMA_HISTOGRAM_ENUMERATION("AutoFill.ServerQueryResponse", type,
                            NUM_SERVER_QUERY_METRICS);
}

}  // namespace autofill_metrics
