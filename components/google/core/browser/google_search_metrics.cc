// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_search_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"

GoogleSearchMetrics::GoogleSearchMetrics() {
}

GoogleSearchMetrics::~GoogleSearchMetrics() {
}

void GoogleSearchMetrics::RecordGoogleSearch(AccessPoint ap) const {
  DCHECK_NE(AP_BOUNDARY, ap);
  UMA_HISTOGRAM_ENUMERATION("GoogleSearch.AccessPoint", ap, AP_BOUNDARY);
}

#if defined(OS_ANDROID)
void GoogleSearchMetrics::RecordAndroidGoogleSearch(
    AccessPoint ap,
    bool prerender_enabled) const {
  DCHECK_NE(AP_BOUNDARY, ap);
  if (prerender_enabled) {
    UMA_HISTOGRAM_ENUMERATION("GoogleSearch.AccessPoint_PrerenderEnabled",
                              ap, AP_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("GoogleSearch.AccessPoint_PrerenderDisabled",
                              ap, AP_BOUNDARY);
  }
}
#endif
