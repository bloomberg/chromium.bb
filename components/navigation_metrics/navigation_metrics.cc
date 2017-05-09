// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "url/gurl.h"

namespace {

// This enum is used in building the histogram. So, this is append only,
// any new scheme should be added at the end, before SCHEME_MAX
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
  SCHEME_BLOB,
  SCHEME_FILESYSTEM,
  SCHEME_MAX,
};

const char* const kSchemeNames[] = {
    "unknown",
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kFileScheme,
    url::kFtpScheme,
    url::kDataScheme,
    url::kJavaScriptScheme,
    url::kAboutScheme,
    "chrome",
    url::kBlobScheme,
    url::kFileSystemScheme,
    "max",
};

static_assert(arraysize(kSchemeNames) == SCHEME_MAX + 1,
              "kSchemeNames should have SCHEME_MAX + 1 elements");

}  // namespace

namespace navigation_metrics {

void RecordMainFrameNavigation(const GURL& url,
                               bool is_same_document,
                               bool is_off_the_record) {
  Scheme scheme = SCHEME_UNKNOWN;
  for (int i = 1; i < SCHEME_MAX; ++i) {
    if (url.SchemeIs(kSchemeNames[i])) {
      scheme = static_cast<Scheme>(i);
      break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameScheme", scheme, SCHEME_MAX);
  if (!is_same_document) {
    UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameSchemeDifferentPage", scheme,
                              SCHEME_MAX);
  }

  if (is_off_the_record) {
    UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameSchemeOTR", scheme,
                              SCHEME_MAX);
    if (!is_same_document) {
      UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameSchemeDifferentPageOTR",
                                scheme, SCHEME_MAX);
    }
  }
}

}  // namespace navigation_metrics
