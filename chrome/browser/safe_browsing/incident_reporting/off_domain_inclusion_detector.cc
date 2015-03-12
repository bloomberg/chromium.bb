// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/off_domain_inclusion_detector.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace safe_browsing {

// A static data structure to carry information specific to a given off-domain
// inclusions across threads, this data will be readable even after the
// underlying request has been deleted.
struct OffDomainInclusionDetector::OffDomainInclusionInfo {
  OffDomainInclusionInfo()
      : resource_type(content::RESOURCE_TYPE_LAST_TYPE), render_process_id(0) {}

  content::ResourceType resource_type;

  // ID of the render process from which this inclusion originates, used to
  // figure out which profile this request belongs to (from the UI thread) if
  // later required during this analysis.
  int render_process_id;

  // The URL of the off-domain inclusion.
  GURL request_url;

  // The URL of the main frame the inclusion was requested by.
  GURL main_frame_url;

  // Cache of the top-level domain contained in |main_frame_url|.
  std::string main_frame_domain;

 private:
  DISALLOW_COPY_AND_ASSIGN(OffDomainInclusionInfo);
};

OffDomainInclusionDetector::OffDomainInclusionDetector(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : OffDomainInclusionDetector(database_manager,
                                 ReportAnalysisEventCallback()) {
}

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
  if (!ShouldAnalyzeRequest(request))
    return;

  scoped_ptr<OffDomainInclusionInfo> off_domain_inclusion_info(
      new OffDomainInclusionInfo);

  const content::ResourceRequestInfo* resource_request_info =
      content::ResourceRequestInfo::ForRequest(request);

  off_domain_inclusion_info->resource_type =
      resource_request_info->GetResourceType();

  off_domain_inclusion_info->render_process_id =
      resource_request_info->GetChildID();

  off_domain_inclusion_info->request_url = request->url();

  // Only requests for subresources of the main frame are analyzed and the
  // referrer URL is always the URL of the main frame itself in such requests.
  off_domain_inclusion_info->main_frame_url = GURL(request->referrer());

  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&OffDomainInclusionDetector::BeginAnalysis,
                            weak_ptr_factory_.GetWeakPtr(),
                            base::Passed(&off_domain_inclusion_info)));
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
      // Types above are to be analyzed for off-domain inclusion if they are
      // loaded as part of the main frame.
      return request_info->IsMainFrame();
    case content::RESOURCE_TYPE_WORKER:
    case content::RESOURCE_TYPE_SHARED_WORKER:
    case content::RESOURCE_TYPE_PREFETCH:
    case content::RESOURCE_TYPE_FAVICON:
    case content::RESOURCE_TYPE_PING:
    case content::RESOURCE_TYPE_SERVICE_WORKER:
      // Types above are not to be analyzed for off-domain inclusion.
      return false;
    case content::RESOURCE_TYPE_LAST_TYPE:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return false;
}

void OffDomainInclusionDetector::BeginAnalysis(
    scoped_ptr<OffDomainInclusionInfo> off_domain_inclusion_info) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (!off_domain_inclusion_info->main_frame_url.is_valid()) {
    // A live experiment confirmed that the only reason the |main_frame_url|
    // would be invalid is if it's empty. The |main_frame_url| can be empty in
    // a few scenarios where the referrer is dropped (e.g., HTTPS => HTTP
    // requests). Consider adding the original referrer to ResourceRequestInfo
    // if that's an issue.
    DCHECK(off_domain_inclusion_info->main_frame_url.is_empty());
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::ABORT_EMPTY_MAIN_FRAME_URL);
    return;
  }

  off_domain_inclusion_info->main_frame_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          off_domain_inclusion_info->main_frame_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (!off_domain_inclusion_info->request_url.DomainIs(
          off_domain_inclusion_info->main_frame_domain.c_str())) {
    // Upon detecting that |request_url| is an off-domain inclusion, compare it
    // against the inclusion whitelist.
    // Note: |request_url| sadly needs to be copied below as the scoped_ptr
    // currently owning it will be passed on to a temporary/unaccessible
    // scoped_ptr in the Reply Callback and a ConstRef of it therefore can't be
    // used in the Task Callback.
    const GURL copied_request_url(off_domain_inclusion_info->request_url);
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseManager::MatchInclusionWhitelistUrl,
                   database_manager_, copied_request_url),
        base::Bind(
            &OffDomainInclusionDetector::ContinueAnalysisOnWhitelistResult,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&off_domain_inclusion_info)));
  }
}

void OffDomainInclusionDetector::ContinueAnalysisOnWhitelistResult(
    scoped_ptr<const OffDomainInclusionInfo> off_domain_inclusion_info,
    bool request_url_is_on_inclusion_whitelist) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (request_url_is_on_inclusion_whitelist) {
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED);
  } else {
    ContinueAnalysisWithHistoryCheck(off_domain_inclusion_info.Pass());
  }
}

void OffDomainInclusionDetector::ContinueAnalysisWithHistoryCheck(
    scoped_ptr<const OffDomainInclusionInfo> off_domain_inclusion_info) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // The |main_thread_task_runner_| is the UI thread in practice currently so
  // this call can be made synchronously, but this would need to be made
  // asynchronous should this ever change.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile =
      ProfileFromRenderProcessId(off_domain_inclusion_info->render_process_id);
  if (!profile) {
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::ABORT_NO_PROFILE);
    return;
  } else if (profile->IsOffTheRecord()) {
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::ABORT_INCOGNITO);
    return;
  }

  HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);

  if (!history_service) {
    // Should only happen very early during startup, receiving many such reports
    // relative to other report types would indicate that something is wrong.
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::ABORT_NO_HISTORY_SERVICE);
    return;
  }

  // Need to explicitly copy |request_url| here or this code would depend on
  // parameter evaluation order as |off_domain_inclusion_info| is being handed
  // off.
  const GURL copied_request_url(off_domain_inclusion_info->request_url);
  history_service->GetVisibleVisitCountToHost(
      copied_request_url,
      base::Bind(&OffDomainInclusionDetector::ContinueAnalysisOnHistoryResult,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&off_domain_inclusion_info)),
      &history_task_tracker_);
}

void OffDomainInclusionDetector::ContinueAnalysisOnHistoryResult(
    scoped_ptr<const OffDomainInclusionInfo> off_domain_inclusion_info,
    bool success,
    int num_visits,
    base::Time /* first_visit_time */) {
  if (!success) {
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::ABORT_HISTORY_LOOKUP_FAILED);
    return;
  }

  if (num_visits > 0) {
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::OFF_DOMAIN_INCLUSION_IN_HISTORY);
  } else {
    ReportAnalysisResult(off_domain_inclusion_info.Pass(),
                         AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS);
  }
}

void OffDomainInclusionDetector::ReportAnalysisResult(
    scoped_ptr<const OffDomainInclusionInfo> off_domain_inclusion_info,
    AnalysisEvent analysis_event) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  // Always record this histogram for the resource type analyzed to be able to
  // do ratio analysis w.r.t. other histograms below.
  UMA_HISTOGRAM_ENUMERATION("SBOffDomainInclusion.RequestAnalyzed",
                            off_domain_inclusion_info->resource_type,
                            content::RESOURCE_TYPE_LAST_TYPE);

  // Log a histogram for the analysis result along with the associated
  // ResourceType.
  std::string histogram_name;
  switch (analysis_event) {
    case AnalysisEvent::NO_EVENT:
      break;
    case AnalysisEvent::ABORT_EMPTY_MAIN_FRAME_URL:
      histogram_name = "SBOffDomainInclusion.Abort.EmptyMainFrameURL";
      break;
    case AnalysisEvent::ABORT_NO_PROFILE:
      histogram_name = "SBOffDomainInclusion.Abort.NoProfile";
      break;
    case AnalysisEvent::ABORT_INCOGNITO:
      histogram_name = "SBOffDomainInclusion.Abort.Incognito";
      break;
    case AnalysisEvent::ABORT_NO_HISTORY_SERVICE:
      histogram_name = "SBOffDomainInclusion.Abort.NoHistoryService";
      break;
    case AnalysisEvent::ABORT_HISTORY_LOOKUP_FAILED:
      histogram_name = "SBOffDomainInclusion.Abort.HistoryLookupFailed";
      break;
    case AnalysisEvent::OFF_DOMAIN_INCLUSION_WHITELISTED:
      histogram_name = "SBOffDomainInclusion.Whitelisted";
      break;
    case AnalysisEvent::OFF_DOMAIN_INCLUSION_IN_HISTORY:
      histogram_name = "SBOffDomainInclusion.InHistory";
      break;
    case AnalysisEvent::OFF_DOMAIN_INCLUSION_SUSPICIOUS:
      histogram_name = "SBOffDomainInclusion.Suspicious";
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
        ->Add(off_domain_inclusion_info->resource_type);
  }

  if (!report_analysis_event_callback_.is_null() &&
      analysis_event != AnalysisEvent::NO_EVENT) {
    report_analysis_event_callback_.Run(analysis_event);
  }
}

}  // namespace safe_browsing
