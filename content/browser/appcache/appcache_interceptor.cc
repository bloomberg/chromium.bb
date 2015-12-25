// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_interceptor.h"

#include "content/browser/appcache/appcache_backend_impl.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_url_request_job.h"
#include "content/common/appcache_interfaces.h"
#include "net/url_request/url_request.h"

static int kHandlerKey;  // Value is not used.

namespace content {

void AppCacheInterceptor::SetHandler(net::URLRequest* request,
                                     AppCacheRequestHandler* handler) {
  request->SetUserData(&kHandlerKey, handler);  // request takes ownership
}

AppCacheRequestHandler* AppCacheInterceptor::GetHandler(
    net::URLRequest* request) {
  return static_cast<AppCacheRequestHandler*>(
      request->GetUserData(&kHandlerKey));
}

void AppCacheInterceptor::SetExtraRequestInfo(
    net::URLRequest* request,
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

  // Create a handler for this request and associate it with the request.
  AppCacheRequestHandler* handler =
      host->CreateRequestHandler(request, resource_type, should_reset_appcache);
  if (handler)
    SetHandler(request, handler);
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
    int new_host_id) {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return;
  DCHECK_NE(kAppCacheNoHostId, new_host_id);
  handler->CompleteCrossSiteTransfer(new_process_id,
                                     new_host_id);
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
  return handler->MaybeLoadResource(request, network_delegate);
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForRedirect(
      request, network_delegate, location);
}

net::URLRequestJob* AppCacheInterceptor::MaybeInterceptResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  AppCacheRequestHandler* handler = GetHandler(request);
  if (!handler)
    return NULL;
  return handler->MaybeLoadFallbackForResponse(request, network_delegate);
}

}  // namespace content
