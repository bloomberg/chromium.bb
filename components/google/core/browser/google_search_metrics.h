// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_SEARCH_METRICS_H_
#define COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_SEARCH_METRICS_H_

#include "build/build_config.h"

// A thin helper class used by parties interested in reporting Google search
// metrics (mostly counts of searches from different access points). This class
// partly exists to make testing easier.
class GoogleSearchMetrics {
 public:
  // Various Google Search access points, to be used with UMA enumeration
  // histograms.
  enum AccessPoint {
    AP_OMNIBOX,
    AP_OMNIBOX_INSTANT,
    AP_DIRECT_NAV,
    AP_DIRECT_NAV_INSTANT,
    AP_HOME_PAGE,
    AP_HOME_PAGE_INSTANT,
    AP_SEARCH_APP,
    AP_SEARCH_APP_INSTANT,
    AP_OTHER,
    AP_OTHER_INSTANT,
    AP_BOUNDARY,
  };

  GoogleSearchMetrics();
  virtual ~GoogleSearchMetrics();

  // Record a single Google search from source |ap|.
  virtual void RecordGoogleSearch(AccessPoint ap) const;

#if defined(OS_ANDROID)
  // Record a single Android Google search from source |ap|. |prerender_enabled|
  // is set to true when prerendering is enabled via settings.
  virtual void RecordAndroidGoogleSearch(AccessPoint ap,
                                         bool prerender_enabled) const;
#endif
};

#endif  // COMPONENTS_GOOGLE_CORE_BROWSER_GOOGLE_SEARCH_METRICS_H_
