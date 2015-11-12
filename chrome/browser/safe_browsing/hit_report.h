// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Datastructures that hold details of a Safe Browsing hit for reporting.

#ifndef CHROME_BROWSER_SAFE_BROWSING_HIT_REPORT_H_
#define CHROME_BROWSER_SAFE_BROWSING_HIT_REPORT_H_

#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "url/gurl.h"

namespace safe_browsing {

// What service classified this threat as unsafe.
enum class ThreatSource {
  UNKNOWN,
  DATA_SAVER,   // From the Data Reduction service.
  LOCAL_PVER3,  // From LocalSafeBrowingDatabaseManager, protocol v3
  LOCAL_PVER4,  // From LocalSafeBrowingDatabaseManager, protocol v4
  REMOTE,       // From RemoteSafeBrowingDatabaseManager
};

// Data to report about the contents of a particular threat (malware, phishing,
// unsafe download URL).  If post_data is non-empty, the request will be
// sent as a POST instead of a GET.
struct HitReport {
  HitReport();
  ~HitReport();

  GURL malicious_url;
  GURL page_url;
  GURL referrer_url;

  bool is_subresource;
  SBThreatType threat_type;
  ThreatSource threat_source;
  bool is_extended_reporting;
  bool is_metrics_reporting_active;

  std::string post_data;
};

// Return true if the user has opted in to UMA metrics reporting.
// Used when filling out a HitReport.
bool IsMetricsReportingActive();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_HIT_REPORT_H_
