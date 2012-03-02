// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/metrics/metrics_service.h"

GoogleSearchMetrics::GoogleSearchMetrics() {
}

GoogleSearchMetrics::~GoogleSearchMetrics() {
}

void GoogleSearchMetrics::RecordGoogleSearch(AccessPoint ap) const {
  DCHECK_NE(AP_BOUNDARY, ap);
  UMA_HISTOGRAM_ENUMERATION("GoogleSearch.AccessPoint", ap, AP_BOUNDARY);
}
