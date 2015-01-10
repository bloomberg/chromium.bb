// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OFF_DOMAIN_INCLUSION_DETECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OFF_DOMAIN_INCLUSION_DETECTOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/common/resource_type.h"

class SafeBrowsingDatabaseManager;

namespace net {
class URLRequest;
}

namespace safe_browsing {

// Observes network requests and reports suspicious activity.
class OffDomainInclusionDetector {
 public:
  enum class AnalysisEvent {
    NO_EVENT,
    EMPTY_MAIN_FRAME_URL,
    INVALID_MAIN_FRAME_URL,
    OFF_DOMAIN_INCLUSION_WHITELISTED,
    OFF_DOMAIN_INCLUSION_SUSPICIOUS,
  };

  // TODO(gab): Hook the OffDomainInclusionDetector to the
  // IncidentReportingService and use an AddIncidentCallback instead of this
  // custom callback type.
  typedef base::Callback<void(AnalysisEvent event)> ReportAnalysisEventCallback;

  explicit OffDomainInclusionDetector(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager);

  // Constructs an OffDomainInclusionDetector with a ReportAnalysisEventCallback
  // to get feedback from detection events, used only in tests.
  OffDomainInclusionDetector(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      const ReportAnalysisEventCallback& report_analysis_event_callback);

  ~OffDomainInclusionDetector();

  // Analyzes the |request| and reports via UMA if an AnalysisEvent is triggered
  // (and via |report_analysis_event_callback_| if it is set).
  void OnResourceRequest(const net::URLRequest* request);

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  const ReportAnalysisEventCallback report_analysis_event_callback_;

  DISALLOW_COPY_AND_ASSIGN(OffDomainInclusionDetector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OFF_DOMAIN_INCLUSION_DETECTOR_H_
