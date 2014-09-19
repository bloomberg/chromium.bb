// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resource_prefetch_predictor_observer.h"

#include <string>

#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using content::BrowserThread;
using predictors::ResourcePrefetchPredictor;

namespace {

// Enum for measuring statistics pertaining to observed request, responses and
// redirects.
enum RequestStats {
  REQUEST_STATS_TOTAL_RESPONSES = 0,
  REQUEST_STATS_TOTAL_PROCESSED_RESPONSES = 1,
  REQUEST_STATS_NO_RESOURCE_REQUEST_INFO = 2,
  REQUEST_STATS_NO_RENDER_FRAME_ID_FROM_REQUEST_INFO = 3,
  REQUEST_STATS_MAX = 4,
};

// Specific to main frame requests.
enum MainFrameRequestStats {
  MAIN_FRAME_REQUEST_STATS_TOTAL_REQUESTS = 0,
  MAIN_FRAME_REQUEST_STATS_PROCESSED_REQUESTS = 1,
  MAIN_FRAME_REQUEST_STATS_TOTAL_REDIRECTS = 2,
  MAIN_FRAME_REQUEST_STATS_PROCESSED_REDIRECTS = 3,
  MAIN_FRAME_REQUEST_STATS_TOTAL_RESPONSES = 4,
  MAIN_FRAME_REQUEST_STATS_PROCESSED_RESPONSES = 5,
  MAIN_FRAME_REQUEST_STATS_MAX = 6,
};

void ReportRequestStats(RequestStats stat) {
  UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.RequestStats",
                            stat,
                            REQUEST_STATS_MAX);
}

void ReportMainFrameRequestStats(MainFrameRequestStats stat) {
  UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.MainFrameRequestStats",
                            stat,
                            MAIN_FRAME_REQUEST_STATS_MAX);
}

bool SummarizeResponse(net::URLRequest* request,
                       ResourcePrefetchPredictor::URLRequestSummary* summary) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info) {
    ReportRequestStats(REQUEST_STATS_NO_RESOURCE_REQUEST_INFO);
    return false;
  }

  int render_process_id, render_frame_id;
  if (!info->GetAssociatedRenderFrame(&render_process_id, &render_frame_id)) {
    ReportRequestStats(REQUEST_STATS_NO_RENDER_FRAME_ID_FROM_REQUEST_INFO);
    return false;
  }

  summary->navigation_id.render_process_id = render_process_id;
  summary->navigation_id.render_frame_id = render_frame_id;
  summary->navigation_id.main_frame_url = request->first_party_for_cookies();
  summary->navigation_id.creation_time = request->creation_time();
  summary->resource_url = request->original_url();
  summary->resource_type = info->GetResourceType();
  request->GetMimeType(&summary->mime_type);
  summary->was_cached = request->was_cached();

  // Use the mime_type to determine the resource type for subresources since
  // types such as PREFETCH, SUB_RESOURCE, etc are not useful.
  if (summary->resource_type != content::RESOURCE_TYPE_MAIN_FRAME) {
    summary->resource_type =
        ResourcePrefetchPredictor::GetResourceTypeFromMimeType(
            summary->mime_type,
            summary->resource_type);
  }
  return true;
}

}  // namespace

namespace chrome_browser_net {

ResourcePrefetchPredictorObserver::ResourcePrefetchPredictorObserver(
    ResourcePrefetchPredictor* predictor)
    : predictor_(predictor->AsWeakPtr()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ResourcePrefetchPredictorObserver::~ResourcePrefetchPredictorObserver() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
        BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ResourcePrefetchPredictorObserver::OnRequestStarted(
    net::URLRequest* request,
    content::ResourceType resource_type,
    int child_id,
    int frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
    ReportMainFrameRequestStats(MAIN_FRAME_REQUEST_STATS_TOTAL_REQUESTS);

  if (!ResourcePrefetchPredictor::ShouldRecordRequest(request, resource_type))
    return;

  ResourcePrefetchPredictor::URLRequestSummary summary;
  summary.navigation_id.render_process_id = child_id;
  summary.navigation_id.render_frame_id = frame_id;
  summary.navigation_id.main_frame_url = request->first_party_for_cookies();
  summary.resource_url = request->original_url();
  summary.resource_type = resource_type;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::RecordURLRequest,
                 predictor_,
                 summary));

  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
    ReportMainFrameRequestStats(MAIN_FRAME_REQUEST_STATS_PROCESSED_REQUESTS);
}

void ResourcePrefetchPredictorObserver::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (request_info &&
      request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    ReportMainFrameRequestStats(MAIN_FRAME_REQUEST_STATS_TOTAL_REDIRECTS);
  }

  if (!ResourcePrefetchPredictor::ShouldRecordRedirect(request))
    return;

  ResourcePrefetchPredictor::URLRequestSummary summary;
  if (!SummarizeResponse(request, &summary))
    return;

  summary.redirect_url = redirect_url;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::RecordURLRedirect,
                 predictor_,
                 summary));

  if (request_info &&
      request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    ReportMainFrameRequestStats(MAIN_FRAME_REQUEST_STATS_PROCESSED_REDIRECTS);
  }
}

void ResourcePrefetchPredictorObserver::OnResponseStarted(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ReportRequestStats(REQUEST_STATS_TOTAL_RESPONSES);

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (request_info &&
      request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    ReportMainFrameRequestStats(MAIN_FRAME_REQUEST_STATS_TOTAL_RESPONSES);
  }

  if (!ResourcePrefetchPredictor::ShouldRecordResponse(request))
    return;
  ResourcePrefetchPredictor::URLRequestSummary summary;
  if (!SummarizeResponse(request, &summary))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::RecordURLResponse,
                 predictor_,
                 summary));

  ReportRequestStats(REQUEST_STATS_TOTAL_PROCESSED_RESPONSES);
  if (request_info &&
      request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    ReportMainFrameRequestStats(MAIN_FRAME_REQUEST_STATS_PROCESSED_RESPONSES);
  }
}

}  // namespace chrome_browser_net
