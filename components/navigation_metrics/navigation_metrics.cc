// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include "base/metrics/histogram.h"
#include "url/gurl.h"

namespace {

enum Scheme {
  SCHEME_UNKNOWN,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FILE,
  SCHEME_FTP,
  SCHEME_DATA,
  SCHEME_JAVASCRIPT,
  SCHEME_ABOUT,
  SCHEME_CHROME,
  SCHEME_MAX,
};

static const char* kSchemeNames[] = {
  "unknown",
  "http",
  "https",
  "file",
  "ftp",
  "data",
  "javascript",
  "about",
  "chrome",
  "max",
};

COMPILE_ASSERT(arraysize(kSchemeNames) == SCHEME_MAX + 1,
               NavigationMetricsRecorder_name_count_mismatch);

}  // namespace

namespace navigation_metrics {

void RecordMainFrameNavigation(const GURL& url) {
  Scheme scheme = SCHEME_UNKNOWN;
  for (int i = 1; i < SCHEME_MAX; ++i) {
    if (url.SchemeIs(kSchemeNames[i])) {
      scheme = static_cast<Scheme>(i);
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.MainFrameScheme", scheme, SCHEME_MAX);
}

}  // namespace navigation_metrics
