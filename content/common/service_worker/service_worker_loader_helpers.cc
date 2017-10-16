// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_loader_helpers.h"

#include "base/strings/stringprintf.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/resource_response.h"
#include "net/http/http_util.h"

namespace content {

// static
std::unique_ptr<ServiceWorkerFetchRequest>
ServiceWorkerLoaderHelpers::CreateFetchRequest(const ResourceRequest& request) {
  std::unique_ptr<ServiceWorkerFetchRequest> new_request =
      base::MakeUnique<ServiceWorkerFetchRequest>();
  new_request->mode = request.fetch_request_mode;
  new_request->is_main_resource_load =
      ServiceWorkerUtils::IsMainResourceType(request.resource_type);
  new_request->request_context_type = request.fetch_request_context_type;
  new_request->frame_type = request.fetch_frame_type;
  new_request->url = request.url;
  new_request->method = request.method;
  // |blob_uuid| and |blob_size| aren't used in MojoBlobs, so just clear them.
  // The caller is responsible for setting the MojoBlob field |blob| if needed.
  DCHECK(features::IsMojoBlobsEnabled());
  new_request->blob_uuid.clear();
  new_request->blob_size = 0;
  new_request->credentials_mode = request.fetch_credentials_mode;
  new_request->redirect_mode = request.fetch_redirect_mode;
  new_request->is_reload = ui::PageTransitionCoreTypeIs(
      request.transition_type, ui::PAGE_TRANSITION_RELOAD);
  new_request->referrer =
      Referrer(GURL(request.referrer), request.referrer_policy);
  new_request->fetch_type = ServiceWorkerFetchType::FETCH;
  return new_request;
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseHeaders(
    const int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers,
    ResourceResponseHead* out_head) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                     status_text.c_str()));
  for (const auto& item : headers) {
    buf.append(item.first);
    buf.append(": ");
    buf.append(item.second);
    buf.append("\r\n");
  }
  buf.append("\r\n");

  out_head->headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(buf.c_str(), buf.size()));
  if (out_head->mime_type.empty()) {
    std::string mime_type;
    out_head->headers->GetMimeType(&mime_type);
    if (mime_type.empty())
      mime_type = "text/plain";
    out_head->mime_type = mime_type;
  }
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseInfo(
    const ServiceWorkerResponse& response,
    ResourceResponseHead* out_head) {
  out_head->was_fetched_via_service_worker = true;
  out_head->was_fetched_via_foreign_fetch = false;
  out_head->was_fallback_required_by_service_worker = false;
  out_head->url_list_via_service_worker = response.url_list;
  out_head->response_type_via_service_worker = response.response_type;
  out_head->is_in_cache_storage = response.is_in_cache_storage;
  out_head->cache_storage_cache_name = response.cache_storage_cache_name;
  out_head->cors_exposed_header_names = response.cors_exposed_header_names;
  out_head->did_service_worker_navigation_preload = false;
}

// static
base::Optional<net::RedirectInfo>
ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
    const ResourceRequest& original_request,
    const ResourceResponseHead& response_head,
    bool token_binding_negotiated) {
  std::string new_location;
  if (!response_head.headers->IsRedirect(&new_location))
    return base::nullopt;

  std::string referrer_string;
  net::URLRequest::ReferrerPolicy referrer_policy;
  Referrer::ComputeReferrerInfo(
      &referrer_string, &referrer_policy,
      Referrer(original_request.referrer, original_request.referrer_policy));

  // If the request is a MAIN_FRAME request, the first-party URL gets
  // updated on redirects.
  const net::URLRequest::FirstPartyURLPolicy first_party_url_policy =
      original_request.resource_type == RESOURCE_TYPE_MAIN_FRAME
          ? net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies, first_party_url_policy,
      referrer_policy, referrer_string, response_head.headers.get(),
      response_head.headers->response_code(),
      original_request.url.Resolve(new_location), token_binding_negotiated);
}

}  // namespace content
