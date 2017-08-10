// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_interceptor.h"

#include <utility>

#include "base/debug/crash_logging.h"
#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_url_request.h"
#include "content/browser/appcache/appcache_url_request_job.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/bad_message.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_requester_info.h"
#include "content/common/appcache_interfaces.h"
#include "net/url_request/url_request.h"

static int kHandlerKey;  // Value is not used.

namespace content {

void AppCacheInterceptor::SetHandler(
    net::URLRequest* request,
    std::unique_ptr<AppCacheRequestHandler> handler) {
  request->SetUserData(&kHandlerKey, std::move(handler));
}

AppCacheRequestHandler* AppCacheInterceptor::GetHandler(
    net::URLRequest* request) {
  return static_cast<AppCacheRequestHandler*>(
      request->GetUserData(&kHandlerKey));
}

void AppCacheInterceptor::SetExtraRequestInfo(net::URLRequest* request,
                                              AppCacheServiceImpl* service,
                                              int process_id,
                                              int host_id,
                                              ResourceType resource_type,
                                              bool should_reset_appcache) {
  if (!service || (host_id == kAppCacheNoHostId))
    return;

  AppCacheBackendImpl* backend = service->GetBackend(process_id);
  if (!backend)
    return;

  // TODO(michaeln): An invalid host id is indicative of bad data
  // from a child process. How should we handle that here?
  AppCacheHost* host = backend->GetHost(host_id);
  if (!host)
    return;

  SetExtraRequestInfoForHost(request, host, resource_type,
                             should_reset_appcache);
}

void AppCacheInterceptor::SetExtraRequestInfoForHost(
    net::URLRequest* request,
    AppCacheHost* host,
    ResourceType resource_type,
    bool should_reset_appcache) {
  // Create a handler for this request and associate it with the request.
  std::unique_ptr<AppCacheRequestHandler> handler =
      host->CreateRequestHandler(AppCacheURLRequest::Create(request),
                                 resource_type, should_reset_appcache);
  if (handler)
    SetHandler(request, std::move(handler));
}

void AppCacheInterceptor::GetExtraResponseInfo(net::URLRequest* request,
                                               int64_t* cache_id,
                                               GURL* manifest_url) {
  DCHECK(*cache_id == kAppCacheNoCacheId);
  DCHECK(manifest_url->is_empty());
  AppCacheRequestHandler* handler = GetHandler(request);
  if (handler)
    handler->GetExtraResponseInfo(cache_id, manifest_url);
}

void AppCacheInterceptor::PrepareForCrossSiteTransfer(
    net::URLRequest* request,
    int old_process_id) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return;
  handler->PrepareForCrossSiteTransfer(old_process_id);
}

void AppCacheInterceptor::CompleteCrossSiteTransfer(
    net::URLRequest* request,
    int new_process_id,
    int new_host_id,
    ResourceRequesterInfo* requester_info) {
  // AppCache is supported only for renderer initiated requests.
  DCHECK(requester_info->IsRenderer());
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return;
  if (!handler->SanityCheckIsSameService(requester_info->appcache_service())) {
    // This can happen when V2 apps and web pages end up in the same storage
    // partition.
    const GURL& first_party_url_for_cookies = request->site_for_cookies();
    if (first_party_url_for_cookies.is_valid()) {
      // TODO(lazyboy): Remove this once we know which extensions run into this
      // issue. See https://crbug.com/612711#c25 for details.
      base::debug::SetCrashKeyValue("aci_wrong_sp_extension_id",
                                    first_party_url_for_cookies.host());
      // No need to explicitly call DumpWithoutCrashing(), since
      // bad_message::ReceivedBadMessage() below will do that.
    }
    bad_message::ReceivedBadMessage(requester_info->filter(),
                                    bad_message::ACI_WRONG_STORAGE_PARTITION);
    return;
  }
  DCHECK_NE(kAppCacheNoHostId, new_host_id);
  handler->CompleteCrossSiteTransfer(new_process_id, new_host_id);
}

void AppCacheInterceptor::MaybeCompleteCrossSiteTransferInOldProcess(
    net::URLRequest* request,
    int process_id) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return;
  handler->MaybeCompleteCrossSiteTransferInOldProcess(process_id);
}

AppCacheInterceptor::AppCacheInterceptor() {
}

AppCacheInterceptor::~AppCacheInterceptor() {
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptRequest(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;

  AppCacheJob* job = handler->MaybeLoadResource(network_delegate);
  return job ? job->AsURLRequestJob() : nullptr;
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;

  AppCacheJob* job =
      handler->MaybeLoadFallbackForRedirect(network_delegate, location);
  return job ? job->AsURLRequestJob() : nullptr;
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;

  AppCacheJob* job = handler->MaybeLoadFallbackForResponse(network_delegate);
  return job ? job->AsURLRequestJob() : nullptr;
}

}  // namespace content
