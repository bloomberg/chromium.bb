// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "components/dom_distiller/core/url_constants.h"
#include "url/gurl.h"

namespace {

// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused. Any new scheme should
// be added at the end, before SCHEME_MAX.
enum Scheme {
  SCHEME_UNKNOWN = 0,
  SCHEME_HTTP = 1,
  SCHEME_HTTPS = 2,
  SCHEME_FILE = 3,
  SCHEME_FTP = 4,
  SCHEME_DATA = 5,
  SCHEME_JAVASCRIPT = 6,
  SCHEME_ABOUT = 7,
  SCHEME_CHROME = 8,
  SCHEME_BLOB = 9,
  SCHEME_FILESYSTEM = 10,
  SCHEME_CHROME_NATIVE = 11,
  SCHEME_CHROME_SEARCH = 12,
  SCHEME_CHROME_DISTILLER = 13,
  SCHEME_CHROME_DEVTOOLS = 14,
  SCHEME_CHROME_EXTENSION = 15,
  SCHEME_VIEW_SOURCE = 16,
  SCHEME_EXTERNALFILE = 17,
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
    "chrome-native",
    "chrome-search",
    dom_distiller::kDomDistillerScheme,
    "chrome-devtools",
    "chrome-extension",
    "view-source",
    "externalfile",
};

static_assert(arraysize(kSchemeNames) == SCHEME_MAX,
              "kSchemeNames should have SCHEME_MAX elements");

Scheme GetScheme(const GURL& url) {
  for (int i = 1; i < SCHEME_MAX; ++i) {
    if (url.SchemeIs(kSchemeNames[i]))
      return static_cast<Scheme>(i);
  }
  return SCHEME_UNKNOWN;
}

}  // namespace

namespace navigation_metrics {

void RecordMainFrameNavigation(const GURL& url,
                               bool is_same_document,
                               bool is_off_the_record) {
  Scheme scheme = GetScheme(url);
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

void RecordOmniboxURLNavigation(const GURL& url) {
  UMA_HISTOGRAM_ENUMERATION("Omnibox.URLNavigationScheme", GetScheme(url),
                            SCHEME_MAX);
}

}  // namespace navigation_metrics
