// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/off_domain_inclusion_detector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing_db/database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

OffDomainInclusionDetector::OffDomainInclusionDetector(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : OffDomainInclusionDetector(database_manager,
                                 ReportAnalysisEventCallback()) {}

OffDomainInclusionDetector::OffDomainInclusionDetector(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    const ReportAnalysisEventCallback& report_analysis_event_callback)
    : database_manager_(database_manager),
      report_analysis_event_callback_(report_analysis_event_callback),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DCHECK(database_manager);
}

OffDomainInclusionDetector::~OffDomainInclusionDetector() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
}

void OffDomainInclusionDetector::OnResourceRequest(
    const net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ShouldAnalyzeRequest(request))
    return;

  const content::ResourceRequestInfo* resource_request_info =
      content::ResourceRequestInfo::ForRequest(request);

  const content::ResourceType resource_type =
      resource_request_info->GetResourceType();

  const GURL request_url = request->url();

  // Only requests for subresources of the main frame are analyzed and the
  // referrer URL is always the URL of the main frame itself in such requests.
  const GURL main_frame_url(request->referrer());

  if (!main_frame_url.is_valid()) {
    // A live experiment confirmed that the only reason the |main_frame_url|
    // would be invalid is if it's empty. The |main_frame_url| can be empty in
    // a few scenarios where the referrer is dropped (e.g., HTTPS => HTTP
    // requests). Consider adding the original referrer to ResourceRequestInfo
    // if that's an issue.
    DCHECK(main_frame_url.is_empty());
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::ABORT_EMPTY_MAIN_FRAME_URL);
    return;
  }

  // Cache of the top-level domain contained in |main_frame_url|.  This could
  // be an IP address.
  std::string main_frame_domain;

  const bool main_frame_is_ip = main_frame_url.HostIsIPAddress();
  if (main_frame_is_ip) {
    main_frame_domain = main_frame_url.host();
  } else {
    main_frame_domain = net::registry_controlled_domains::GetDomainAndRegistry(
        main_frame_url,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  }
  const bool request_is_ip = request_url.HostIsIPAddress();

  // Check if this is indeed an off-domain inclusion.
  bool is_off_domain = false;
  if (main_frame_is_ip && request_is_ip) {
    // Both are IP addresses: compare them as strings
    is_off_domain = main_frame_domain != request_url.host();
  } else if (!main_frame_is_ip && !request_is_ip) {
    // Neither are: compare as domains
    is_off_domain = !request_url.DomainIs(main_frame_domain);
  } else {
    // Just one is an IP
    is_off_domain = true;
  }

  if (!is_off_domain)
    return;

  // Upon detecting that |request_url| is an off-domain inclusion, compare it
  // against the inclusion whitelist.
  //
  // This function must be called on the IO thread. The code above could be
  // called from any thread, in theory.
  if (database_manager_->MatchInclusionWhitelistUrl(request_url)) {
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED);
    return;
  }

  // On a negative result from the whitelist, query the HistoryService to see
  // if this url has been seen before.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OffDomainInclusionDetector::ContinueAnalysisWithHistoryCheck,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_type,
                 request_url,
                 resource_request_info->GetChildID()));
}

void OffDomainInclusionDetector::ContinueAnalysisWithHistoryCheck(
    const content::ResourceType resource_type,
    const GURL& request_url,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile =
      ProfileFromRenderProcessId(render_process_id);
  if (!profile) {
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::ABORT_NO_PROFILE);
    return;
  }

  if (profile->IsOffTheRecord()) {
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::ABORT_INCOGNITO);
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);

  if (!history_service) {
    // Should only happen very early during startup, receiving many such reports
    // relative to other report types would indicate that something is wrong.
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::ABORT_NO_HISTORY_SERVICE);
    return;
  }

  history_service->GetVisibleVisitCountToHost(
      request_url,
      base::Bind(&OffDomainInclusionDetector::ContinueAnalysisOnHistoryResult,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_type),
      &history_task_tracker_);
}

void OffDomainInclusionDetector::ContinueAnalysisOnHistoryResult(
    const content::ResourceType resource_type,
    bool success,
    int num_visits,
    base::Time /* first_visit_time */) {
  if (!success) {
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::ABORT_HISTORY_LOOKUP_FAILED);
    return;
  }

  if (num_visits > 0) {
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::OFF_DOMAIN_INCLUSION_IN_HISTORY);
  } else {
    ReportAnalysisResult(resource_type,
                         AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS);
  }
}

Profile* OffDomainInclusionDetector::ProfileFromRenderProcessId(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!render_process_host)
    return nullptr;

  return Profile::FromBrowserContext(render_process_host->GetBrowserContext());
}

// static
bool OffDomainInclusionDetector::ShouldAnalyzeRequest(
    const net::URLRequest* request) {
  // Only look at actual net requests (e.g., not chrome-extensions://id/foo.js).
  if (!request->url().SchemeIsHTTPOrHTTPS())
    return false;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  const content::ResourceType resource_type = request_info->GetResourceType();

  // Explicitly handle every resource type (to force compile errors if a new one
  // is added).
  switch (resource_type) {
    case content::RESOURCE_TYPE_MAIN_FRAME:
      // Analyze inclusions in the main frame, not the main frame itself.
      return false;
    case content::RESOURCE_TYPE_SUB_FRAME:
      DCHECK(!request_info->IsMainFrame());
      // Only analyze top-level frames within the main frame.
      return request_info->ParentIsMainFrame();
    case content::RESOURCE_TYPE_STYLESHEET:
    case content::RESOURCE_TYPE_SCRIPT:
    case content::RESOURCE_TYPE_IMAGE:
    case content::RESOURCE_TYPE_FONT_RESOURCE:
    case content::RESOURCE_TYPE_SUB_RESOURCE:
    case content::RESOURCE_TYPE_OBJECT:
    case content::RESOURCE_TYPE_MEDIA:
    case content::RESOURCE_TYPE_XHR:
    case content::RESOURCE_TYPE_PLUGIN_RESOURCE:
      // Types above are to be analyzed for off-domain inclusion if they are
      // loaded as part of the main frame.
      return request_info->IsMainFrame();
    case content::RESOURCE_TYPE_WORKER:
    case content::RESOURCE_TYPE_SHARED_WORKER:
    case content::RESOURCE_TYPE_PREFETCH:
    case content::RESOURCE_TYPE_FAVICON:
    case content::RESOURCE_TYPE_PING:
    case content::RESOURCE_TYPE_SERVICE_WORKER:
    case content::RESOURCE_TYPE_CSP_REPORT:
      // Types above are not to be analyzed for off-domain inclusion.
      return false;
    case content::RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return false;
}

void OffDomainInclusionDetector::ReportAnalysisResult(
    const content::ResourceType resource_type,
    AnalysisEvent analysis_event) {
  // Always record this histogram for the resource type analyzed to be able to
  // do ratio analysis w.r.t. other histograms below.
  UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion2.RequestAnalyzed",
                            resource_type,
                            content::RESOURCE_TYPE_LAST_TYPE);

  // Log a histogram for the analysis result along with the associated
  // ResourceType.
  std::string histogram_name;
  switch (analysis_event) {
    case AnalysisEvent::NO_EVENT:
      break;
    case AnalysisEvent::ABORT_EMPTY_MAIN_FRAME_URL:
      histogram_name = "SBOffDomainInclusion2.Abort.EmptyMainFrameURL";
      break;
    case AnalysisEvent::ABORT_NO_PROFILE:
      histogram_name = "SBOffDomainInclusion2.Abort.NoProfile";
      break;
    case AnalysisEvent::ABORT_INCOGNITO:
      histogram_name = "SBOffDomainInclusion2.Abort.Incognito";
      break;
    case AnalysisEvent::ABORT_NO_HISTORY_SERVICE:
      histogram_name = "SBOffDomainInclusion2.Abort.NoHistoryService";
      break;
    case AnalysisEvent::ABORT_HISTORY_LOOKUP_FAILED:
      histogram_name = "SBOffDomainInclusion2.Abort.HistoryLookupFailed";
      break;
    case AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED:
      histogram_name = "SBOffDomainInclusion2.Whitelisted";
      break;
    case AnalysisEvent::OFF_DOMAIN_INCLUSION_IN_HISTORY:
      histogram_name = "SBOffDomainInclusion2.InHistory";
      break;
    case AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS:
      histogram_name = "SBOffDomainInclusion2.Suspicious";
      break;
  }
  if (!histogram_name.empty()) {
    // Expanded from the UMA_HISTOGRAM_ENUMERATION macro.
    base::LinearHistogram::FactoryGet(
        histogram_name,
        1,                                     // minimum,
        content::RESOURCE_TYPE_LAST_TYPE,      // maximum
        content::RESOURCE_TYPE_LAST_TYPE + 1,  // bucket_count
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(resource_type);
  }

  if (!report_analysis_event_callback_.is_null() &&
      analysis_event != AnalysisEvent::NO_EVENT) {
    // Accessing report_analysis_event_callback_ is threadsafe since the member
    // is only read and never written after construction. The function it
    // points to may not be threadsafe, though, so always call it on the main
    // thread. (We might already be on the main thread, but this callback should
    // be rare enough that it's not worth testing first.)
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(report_analysis_event_callback_, analysis_event));
  }
}

}  // namespace safe_browsing
