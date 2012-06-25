// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resource_prefetch_predictor_observer.h"

#include <string>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using predictors::ResourcePrefetchPredictor;

namespace {

bool SummarizeResponse(net::URLRequest* request,
                       ResourcePrefetchPredictor::URLRequestSummary* summary) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info) {
    LOG(ERROR) << "No ResourceRequestInfo in request";
    return false;
  }

  int render_process_id, render_view_id;
  if (!info->GetAssociatedRenderView(&render_process_id, &render_view_id)) {
    LOG(ERROR) << "Could not get RenderViewId from request info.";
    return false;
  }

  summary->navigation_id.render_process_id = render_process_id;
  summary->navigation_id.render_view_id = render_view_id;
  summary->navigation_id.main_frame_url = request->first_party_for_cookies();
  summary->navigation_id.creation_time = request->creation_time();
  summary->resource_url = request->original_url();
  summary->resource_type = info->GetResourceType();
  request->GetMimeType(&summary->mime_type);
  summary->was_cached = request->was_cached();

  // We want to rely on the mime_type to determine the resource type since we
  // dont want types such as PREFETCH, SUB_RESOURCE, etc.
  summary->resource_type =
      ResourcePrefetchPredictor::GetResourceTypeFromMimeType(
          summary->mime_type,
          summary->resource_type);
  return true;
}

}  // namespace

namespace chrome_browser_net {

ResourcePrefetchPredictorObserver::ResourcePrefetchPredictorObserver(
    ResourcePrefetchPredictor* predictor)
        : predictor_(predictor->AsWeakPtr()) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ResourcePrefetchPredictorObserver::~ResourcePrefetchPredictorObserver() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ResourcePrefetchPredictorObserver::OnRequestStarted(
    net::URLRequest* request,
    ResourceType::Type resource_type,
    int child_id,
    int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!ResourcePrefetchPredictor::ShouldRecordRequest(request, resource_type))
    return;

  ResourcePrefetchPredictor::URLRequestSummary summary;
  summary.navigation_id.render_process_id = child_id;
  summary.navigation_id.render_view_id = route_id;
  summary.navigation_id.main_frame_url = request->first_party_for_cookies();
  summary.resource_url = request->original_url();
  summary.resource_type = resource_type;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::RecordURLRequest,
                 predictor_,
                 summary));
}

void ResourcePrefetchPredictorObserver::OnRequestRedirected(
    net::URLRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!ResourcePrefetchPredictor::ShouldRecordRedirect(request))
    return;
  ResourcePrefetchPredictor::URLRequestSummary summary;
  if (!SummarizeResponse(request, &summary))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::RecordUrlRedirect,
                 predictor_,
                 summary));
}

void ResourcePrefetchPredictorObserver::OnResponseStarted(
    net::URLRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!ResourcePrefetchPredictor::ShouldRecordResponse(request))
    return;
  ResourcePrefetchPredictor::URLRequestSummary summary;
  if (!SummarizeResponse(request, &summary))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ResourcePrefetchPredictor::RecordUrlResponse,
                 predictor_,
                 summary));
}

}  // namespace chrome_browser_net
