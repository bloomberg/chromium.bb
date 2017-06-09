// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/mime_util/mime_util.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/url_request/url_request.h"

namespace predictors {

namespace {

bool g_allow_port_in_urls = false;

}  // namespace

// static
bool LoadingDataCollector::ShouldRecordRequest(
    net::URLRequest* request,
    content::ResourceType resource_type) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  return resource_type == content::RESOURCE_TYPE_MAIN_FRAME &&
         IsHandledMainPage(request);
}

// static
bool LoadingDataCollector::ShouldRecordResponse(net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  content::ResourceType resource_type = request_info->GetResourceType();
  return resource_type == content::RESOURCE_TYPE_MAIN_FRAME
             ? IsHandledMainPage(response)
             : IsHandledSubresource(response, resource_type);
}

// static
bool LoadingDataCollector::ShouldRecordRedirect(net::URLRequest* response) {
  return ShouldRecordResponse(response);
}

// static
bool LoadingDataCollector::IsHandledMainPage(net::URLRequest* request) {
  const GURL& url = request->url();
  bool bad_port = !g_allow_port_in_urls && url.has_port();
  return url.SchemeIsHTTPOrHTTPS() && !bad_port;
}

// static
bool LoadingDataCollector::IsHandledSubresource(
    net::URLRequest* response,
    content::ResourceType resource_type) {
  const GURL& url = response->url();
  bool bad_port = !g_allow_port_in_urls && url.has_port();
  if (!response->first_party_for_cookies().SchemeIsHTTPOrHTTPS() ||
      !url.SchemeIsHTTPOrHTTPS() || bad_port) {
    return false;
  }

  std::string mime_type;
  response->GetMimeType(&mime_type);
  if (!IsHandledResourceType(resource_type, mime_type))
    return false;

  if (response->method() != "GET")
    return false;

  if (response->original_url().spec().length() >
      ResourcePrefetchPredictorTables::kMaxStringLength) {
    return false;
  }

  if (!response->response_info().headers.get())
    return false;

  return true;
}

// static
bool LoadingDataCollector::IsHandledResourceType(
    content::ResourceType resource_type,
    const std::string& mime_type) {
  content::ResourceType actual_resource_type =
      ResourcePrefetchPredictor::GetResourceType(resource_type, mime_type);
  return actual_resource_type == content::RESOURCE_TYPE_STYLESHEET ||
         actual_resource_type == content::RESOURCE_TYPE_SCRIPT ||
         actual_resource_type == content::RESOURCE_TYPE_IMAGE ||
         actual_resource_type == content::RESOURCE_TYPE_FONT_RESOURCE;
}

// static
void LoadingDataCollector::SetAllowPortInUrlsForTesting(bool state) {
  g_allow_port_in_urls = state;
}

LoadingDataCollector::LoadingDataCollector(ResourcePrefetchPredictor* predictor)
    : predictor_(predictor) {}

LoadingDataCollector::~LoadingDataCollector() {}

void LoadingDataCollector::RecordURLRequest(
    const ResourcePrefetchPredictor::URLRequestSummary& request) {
  predictor_->RecordURLRequest(request);
}

void LoadingDataCollector::RecordURLResponse(
    const ResourcePrefetchPredictor::URLRequestSummary& response) {
  predictor_->RecordURLResponse(response);
}

void LoadingDataCollector::RecordURLRedirect(
    const ResourcePrefetchPredictor::URLRequestSummary& response) {
  predictor_->RecordURLRedirect(response);
}

void LoadingDataCollector::RecordMainFrameLoadComplete(
    const NavigationID& navigation_id) {
  predictor_->RecordMainFrameLoadComplete(navigation_id);
}

void LoadingDataCollector::RecordFirstContentfulPaint(
    const NavigationID& navigation_id,
    const base::TimeTicks& first_contentful_paint) {
  predictor_->RecordFirstContentfulPaint(navigation_id, first_contentful_paint);
}

}  // namespace predictors
