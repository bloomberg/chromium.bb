// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/load_timing_observer.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_handler.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/common/extensions/user_script.h"
#include "content/browser/browser_thread.h"
#include "content/browser/resource_context.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/common/resource_messages.h"
#include "net/base/load_flags.h"

// TODO(oshima): Enable this for other platforms.
#if defined(OS_CHROMEOS)
#include "chrome/browser/renderer_host/offline_resource_handler.h"
#endif

ChromeResourceDispatcherHostDelegate::ChromeResourceDispatcherHostDelegate(
    ResourceDispatcherHost* resource_dispatcher_host,
    prerender::PrerenderTracker* prerender_tracker)
    : resource_dispatcher_host_(resource_dispatcher_host),
      safe_browsing_(g_browser_process->safe_browsing_service()),
      prerender_tracker_(prerender_tracker) {
}

ChromeResourceDispatcherHostDelegate::~ChromeResourceDispatcherHostDelegate() {
}

bool ChromeResourceDispatcherHostDelegate::ShouldBeginRequest(
    int child_id, int route_id,
    const ResourceHostMsg_Request& request_data,
    const content::ResourceContext& resource_context,
    const GURL& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Handle a PREFETCH resource type. If prefetch is disabled, squelch the
  // request.  Otherwise, do a normal request to warm the cache.
  if (request_data.resource_type == ResourceType::PREFETCH) {
    // All PREFETCH requests should be GETs, but be defensive about it.
    if (request_data.method != "GET")
      return false;

    // If prefetch is disabled, kill the request.
    if (!ResourceDispatcherHost::is_prefetch_enabled())
      return false;
  }

  // Handle a PRERENDER motivated request. Very similar to rel=prefetch, these
  // rel=prerender requests instead launch an early render of the entire page.
  if (request_data.resource_type == ResourceType::PRERENDER) {
    if (prerender::PrerenderManager::IsPrerenderingPossible()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          NewRunnableFunction(prerender::HandleTag,
                              resource_context.prerender_manager(),
                              child_id,
                              route_id,
                              request_data.url,
                              referrer));
    }
    // Prerendering or not, this request should be aborted.
    return false;
  }

  // Abort any prerenders that spawn requests that use invalid HTTP methods.
  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id) &&
      !prerender::PrerenderManager::IsValidHttpMethod(request_data.method)) {
    prerender_tracker_->TryCancelOnIOThread(
        child_id, route_id,
        prerender::FINAL_STATUS_INVALID_HTTP_METHOD);
    return false;
  }

  return true;
}

ResourceHandler* ChromeResourceDispatcherHostDelegate::RequestBeginning(
    ResourceHandler* handler,
    net::URLRequest* request,
    bool is_subresource,
    int child_id,
    int route_id) {
  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id))
    request->set_load_flags(request->load_flags() | net::LOAD_PRERENDERING);

  // Insert safe browsing at the front of the chain, so it gets to decide
  // on policies first.
  if (safe_browsing_->enabled()) {
    handler = CreateSafeBrowsingResourceHandler(
        handler, child_id, route_id, is_subresource);
  }

#if defined(OS_CHROMEOS)
  // We check offline first, then check safe browsing so that we still can block
  // unsafe site after we remove offline page.
  handler = new OfflineResourceHandler(
      handler, child_id, route_id, resource_dispatcher_host_, request);
#endif
  return handler;
}

ResourceHandler* ChromeResourceDispatcherHostDelegate::DownloadStarting(
    ResourceHandler* handler,
    int child_id,
    int route_id) {
  if (!safe_browsing_->enabled())
    return handler;

  return CreateSafeBrowsingResourceHandler(handler, child_id, route_id, false);
}

bool ChromeResourceDispatcherHostDelegate::ShouldDeferStart(
    net::URLRequest* request,
    const content::ResourceContext& resource_context) {
  ResourceDispatcherHostRequestInfo* info =
      resource_dispatcher_host_->InfoForRequest(request);
  return prerender_tracker_->PotentiallyDelayRequestOnIOThread(
      request->url(), resource_context.prerender_manager(),
      info->child_id(), info->route_id(), info->request_id());
}

bool ChromeResourceDispatcherHostDelegate::AcceptSSLClientCertificateRequest(
    net::URLRequest* request, net::SSLCertRequestInfo* cert_request_info) {
  if (request->load_flags() & net::LOAD_PREFETCH)
    return false;

  if (request->load_flags() & net::LOAD_PRERENDERING) {
    int child_id, route_id;
    if (ResourceDispatcherHost::RenderViewForRequest(
            request, &child_id, &route_id)) {
      if (prerender_tracker_->TryCancel(
              child_id, route_id,
              prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED)) {
        return false;
      }
    }
  }

  return true;
}

bool ChromeResourceDispatcherHostDelegate::AcceptAuthRequest(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  if (!(request->load_flags() & net::LOAD_PRERENDERING))
    return true;

  int child_id, route_id;
  if (!ResourceDispatcherHost::RenderViewForRequest(
          request, &child_id, &route_id)) {
    NOTREACHED();
    return true;
  }

  if (!prerender_tracker_->TryCancelOnIOThread(
          child_id, route_id, prerender::FINAL_STATUS_AUTH_NEEDED)) {
    return true;
  }

  return false;
}

ResourceDispatcherHostLoginDelegate*
    ChromeResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info, net::URLRequest* request) {
  return CreateLoginPrompt(auth_info, request);
}

void ChromeResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url, int child_id, int route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &ExternalProtocolHandler::LaunchUrl, url, child_id, route_id));
}

ResourceHandler*
    ChromeResourceDispatcherHostDelegate::CreateSafeBrowsingResourceHandler(
        ResourceHandler* handler, int child_id, int route_id,
        bool subresource) {
  return SafeBrowsingResourceHandler::Create(
      handler, child_id, route_id, subresource, safe_browsing_,
      resource_dispatcher_host_);
}

bool ChromeResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url, const std::string& mime_type) {
  // Special-case user scripts to get downloaded instead of viewed.
  return UserScript::IsURLUserScript(url, mime_type);
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request, ResourceResponse* response) {
  LoadTimingObserver::PopulateTimingInfo(request, response);
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    net::URLRequest* request, ResourceResponse* response) {
  LoadTimingObserver::PopulateTimingInfo(request, response);
}
