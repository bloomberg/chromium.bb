// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/off_domain_inclusion_detector.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace safe_browsing {

OffDomainInclusionDetector::OffDomainInclusionDetector(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : OffDomainInclusionDetector(database_manager,
                                 ReportAnalysisEventCallback()) {
}

OffDomainInclusionDetector::OffDomainInclusionDetector(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    const ReportAnalysisEventCallback& report_analysis_event_callback)
    : database_manager_(database_manager),
      report_analysis_event_callback_(report_analysis_event_callback) {
  DCHECK(database_manager);
}

OffDomainInclusionDetector::~OffDomainInclusionDetector() {
}

void OffDomainInclusionDetector::OnResourceRequest(
    const net::URLRequest* request) {
  // Must be called on the IO thread for now as it accesses the safe browsing
  // database manager on it, but the analysis below could be made asynchronous
  // if needed.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Only look at actual net requests (e.g., not chrome-extensions://id/foo.js).
  if (!request->url().SchemeIsHTTPOrHTTPS())
    return;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  const content::ResourceType resource_type = request_info->GetResourceType();

  // Explicitly filter every resource type.
  switch (resource_type) {
    case content::RESOURCE_TYPE_MAIN_FRAME:
      // Analyze inclusions in the main frame, not the main frame itself.
      return;
    case content::RESOURCE_TYPE_SUB_FRAME:
      DCHECK(!request_info->IsMainFrame());
      // Only analyze top-level frames within the main frame.
      if (!request_info->ParentIsMainFrame())
        return;
      break;
    case content::RESOURCE_TYPE_STYLESHEET:
    case content::RESOURCE_TYPE_SCRIPT:
    case content::RESOURCE_TYPE_IMAGE:
    case content::RESOURCE_TYPE_FONT_RESOURCE:
    case content::RESOURCE_TYPE_SUB_RESOURCE:
    case content::RESOURCE_TYPE_OBJECT:
    case content::RESOURCE_TYPE_MEDIA:
    case content::RESOURCE_TYPE_XHR:
      // Types above are to be analyzed for off-domain inclusion if they are
      // loaded as part of the main frame.
      if (!request_info->IsMainFrame())
        return;
      break;
    case content::RESOURCE_TYPE_WORKER:
    case content::RESOURCE_TYPE_SHARED_WORKER:
    case content::RESOURCE_TYPE_PREFETCH:
    case content::RESOURCE_TYPE_FAVICON:
    case content::RESOURCE_TYPE_PING:
    case content::RESOURCE_TYPE_SERVICE_WORKER:
      // Types above are not to be analyzed for off-domain inclusion.
      return;
    case content::RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED();
      return;
  }

  // Record the type of request analyzed to be able to do ratio analysis w.r.t.
  // other histograms below.
  UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion.RequestAnalyzed",
                            resource_type,
                            content::RESOURCE_TYPE_LAST_TYPE);

  AnalysisEvent analysis_event = AnalysisEvent::NO_EVENT;

  const GURL main_frame_url(request->referrer());
  if (!main_frame_url.is_valid()) {
    if (main_frame_url.is_empty()) {
      // This can happen in a few scenarios where the referrer is dropped (e.g.,
      // HTTPS => HTTP requests). Consider adding the original referrer to
      // ResourceRequestInfo if that's an issue.
      UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion.EmptyMainFrameURL",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      analysis_event = AnalysisEvent::EMPTY_MAIN_FRAME_URL;
    } else {
      // There is no reason for the main frame to start loading resources if its
      // own URL is invalid but measure this in the wild to make sure.
      UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion.InvalidMainFrameURL",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      analysis_event = AnalysisEvent::INVALID_MAIN_FRAME_URL;
    }
  } else {
    const std::string main_frame_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            main_frame_url,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    // Off-Domain Inclusion?
    if (!request->url().DomainIs(main_frame_domain.c_str())) {
      // Whitelisted?
      if (database_manager_->MatchInclusionWhitelistUrl(request->url())) {
        UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion.Whitelisted",
                                  resource_type,
                                  content::RESOURCE_TYPE_LAST_TYPE);
        analysis_event = AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED;

      } else {
        UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion.Suspicious",
                                  resource_type,
                                  content::RESOURCE_TYPE_LAST_TYPE);
        analysis_event = AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS;
      }
    }
  }

  if (!report_analysis_event_callback_.is_null() &&
      analysis_event != AnalysisEvent::NO_EVENT) {
    report_analysis_event_callback_.Run(analysis_event);
  }
}

}  // namespace safe_browsing
