// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_METRICS_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_METRICS_H_

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
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_METRICS_H_
