// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_throttling_resource_handler.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/net/load_timing_observer.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_url_request_user_data.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_handler.h"
#include "chrome/browser/renderer_host/transfer_navigation_resource_handler.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/auto_login_prompter.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/resource_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/load_flags.h"
#include "net/base/ssl_config_service.h"

// TODO(oshima): Enable this for other platforms.
#if defined(OS_CHROMEOS)
#include "chrome/browser/renderer_host/offline_resource_handler.h"
#endif

using content::BrowserThread;

namespace {

// Empty ResourceDispatcherHostLoginDelegate implementation used for instant.
// Auth navigations don't commit the load (the load remains pending) until the
// user cancels or succeeds in authorizing. Since we don't allow merging of
// TabContents with pending loads we disallow auth dialogs from showing during
// instant. This empty ResourceDispatcherHostLoginDelegate implementation does
// that.
// TODO: see if we can handle this case more robustly.
class InstantResourceDispatcherHostLoginDelegate
    : public ResourceDispatcherHostLoginDelegate {
 public:
  InstantResourceDispatcherHostLoginDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantResourceDispatcherHostLoginDelegate);
};

void AddPrerenderOnUI(
    int render_process_id, int render_view_id,
    const GURL& url, const content::Referrer& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prerender::PrerenderManager* prerender_manager =
      prerender::FindPrerenderManagerUsingRenderProcessId(render_process_id);
  if (!prerender_manager || !prerender_manager->is_enabled())
    return;

  prerender_manager->AddPrerenderFromLinkRelPrerender(render_process_id,
                                                      render_view_id,
                                                      url,
                                                      referrer);
}

void NotifyDownloadInitiatedOnUI(int render_process_id, int render_view_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(render_process_id,
                                               render_view_id);
  if (!rvh)
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_DOWNLOAD_INITIATED,
      content::Source<RenderViewHost>(rvh),
      content::NotificationService::NoDetails());
}

}  // end namespace

ChromeResourceDispatcherHostDelegate::ChromeResourceDispatcherHostDelegate(
    ResourceDispatcherHost* resource_dispatcher_host,
    prerender::PrerenderTracker* prerender_tracker)
    : resource_dispatcher_host_(resource_dispatcher_host),
      download_request_limiter_(g_browser_process->download_request_limiter()),
      safe_browsing_(g_browser_process->safe_browsing_service()),
      prerender_tracker_(prerender_tracker) {
}

ChromeResourceDispatcherHostDelegate::~ChromeResourceDispatcherHostDelegate() {
}

bool ChromeResourceDispatcherHostDelegate::ShouldBeginRequest(
    int child_id,
    int route_id,
    const std::string& method,
    const GURL& url,
    ResourceType::Type resource_type,
    const content::ResourceContext& resource_context,
    const content::Referrer& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Handle a PREFETCH resource type. If prefetch is disabled, squelch the
  // request.  Otherwise, do a normal request to warm the cache.
  if (resource_type == ResourceType::PREFETCH) {
    // All PREFETCH requests should be GETs, but be defensive about it.
    if (method != "GET")
      return false;

    // If prefetch is disabled, kill the request.
    if (!ResourceDispatcherHost::is_prefetch_enabled())
      return false;
  }

  // Handle a PRERENDER motivated request. Very similar to rel=prefetch, these
  // rel=prerender requests instead launch an early render of the entire page.
  if (resource_type == ResourceType::PRERENDER) {
    if (prerender::PrerenderManager::IsPrerenderingPossible()) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&AddPrerenderOnUI, child_id, route_id, url, referrer));
    }
    // Prerendering or not, this request should be aborted.
    return false;
  }

  // Abort any prerenders that spawn requests that use invalid HTTP methods.
  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id) &&
      !prerender::PrerenderManager::IsValidHttpMethod(method)) {
    prerender_tracker_->TryCancelOnIOThread(
        child_id, route_id, prerender::FINAL_STATUS_INVALID_HTTP_METHOD);
    return false;
  }

  return true;
}

ResourceHandler* ChromeResourceDispatcherHostDelegate::RequestBeginning(
    ResourceHandler* handler,
    net::URLRequest* request,
    const content::ResourceContext& resource_context,
    bool is_subresource,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request) {
  if (is_continuation_of_transferred_request)
    ChromeURLRequestUserData::Delete(request);

  ChromeURLRequestUserData* user_data =
      ChromeURLRequestUserData::Create(request);
  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id)) {
    user_data->set_is_prerender(true);
    request->set_priority(net::IDLE);
  }

#if defined(ENABLE_SAFE_BROWSING)
  // Insert safe browsing at the front of the chain, so it gets to decide
  // on policies first.
  ProfileIOData* io_data = reinterpret_cast<ProfileIOData*>(
      resource_context.GetUserData(NULL));
  if (io_data->safe_browsing_enabled()->GetValue()) {
    handler = CreateSafeBrowsingResourceHandler(
        handler, child_id, route_id, is_subresource);
  }
#endif

#if defined(OS_CHROMEOS)
  // We check offline first, then check safe browsing so that we still can block
  // unsafe site after we remove offline page.
  handler = new OfflineResourceHandler(
      handler, child_id, route_id, resource_dispatcher_host_, request,
      resource_context.appcache_service());
#endif

  // TODO(mpcomplete): Leaving disabled for now, since I'm checking this in
  // close to the branch point.
#if defined(TRANSFER_REDIRECTS_BUG79520)
  handler = new TransferNavigationResourceHandler(
      handler, resource_dispatcher_host_, request);
#endif

  return handler;
}

ResourceHandler* ChromeResourceDispatcherHostDelegate::DownloadStarting(
      ResourceHandler* handler,
      const content::ResourceContext& resource_context,
      net::URLRequest* request,
      int child_id,
      int route_id,
      int request_id,
      bool is_new_request,
      bool in_complete) {

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyDownloadInitiatedOnUI, child_id, route_id));

  // If this isn't a new request, we've seen this before and added the safe
  // browsing resource handler already so no need to add it again. This code
  // path is only hit for requests initiated through the browser, and not the
  // web, so no need to add the throttling handler.
  if (is_new_request) {
#if defined(ENABLE_SAFE_BROWSING)
    ProfileIOData* io_data = reinterpret_cast<ProfileIOData*>(
        resource_context.GetUserData(NULL));
    if (!io_data->safe_browsing_enabled()->GetValue())
      return handler;

    return CreateSafeBrowsingResourceHandler(
        handler, child_id, route_id, false);
#else
    return handler;
#endif
  }

  return new DownloadThrottlingResourceHandler(
      handler, resource_dispatcher_host_, download_request_limiter_, request,
      request->url(), child_id, route_id, request_id, in_complete);
}

bool ChromeResourceDispatcherHostDelegate::ShouldDeferStart(
    net::URLRequest* request,
    const content::ResourceContext& resource_context) {
  ResourceDispatcherHostRequestInfo* info =
      resource_dispatcher_host_->InfoForRequest(request);
  return prerender_tracker_->PotentiallyDelayRequestOnIOThread(
      request->url(), info->child_id(), info->route_id(), info->request_id());
}

bool ChromeResourceDispatcherHostDelegate::AcceptSSLClientCertificateRequest(
    net::URLRequest* request, net::SSLCertRequestInfo* cert_request_info) {
  if (request->load_flags() & net::LOAD_PREFETCH)
    return false;

  ChromeURLRequestUserData* user_data = ChromeURLRequestUserData::Get(request);
  if (user_data && user_data->is_prerender()) {
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
  ChromeURLRequestUserData* user_data = ChromeURLRequestUserData::Get(request);
  if (!user_data || !user_data->is_prerender())
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
  std::string instant_header_value;
  if (request->extra_request_headers().GetHeader(
          InstantLoader::kInstantHeader, &instant_header_value) &&
      instant_header_value == InstantLoader::kInstantHeaderValue)
    return new InstantResourceDispatcherHostLoginDelegate;
  return CreateLoginPrompt(auth_info, request);
}

void ChromeResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url, int child_id, int route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalProtocolHandler::LaunchUrl, url, child_id, route_id));
}

#if defined(ENABLE_SAFE_BROWSING)
ResourceHandler*
    ChromeResourceDispatcherHostDelegate::CreateSafeBrowsingResourceHandler(
        ResourceHandler* handler, int child_id, int route_id,
        bool subresource) {
  return SafeBrowsingResourceHandler::Create(
      handler, child_id, route_id, subresource, safe_browsing_,
      resource_dispatcher_host_);
}
#endif

bool ChromeResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url, const std::string& mime_type) {
  // Special-case user scripts to get downloaded instead of viewed.
  return UserScript::IsURLUserScript(url, mime_type);
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceResponse* response,
    ResourceMessageFilter* filter) {
  LoadTimingObserver::PopulateTimingInfo(request, response);

  ResourceDispatcherHostRequestInfo* info =
      resource_dispatcher_host_->InfoForRequest(request);

  if (request->url().SchemeIsSecure()) {
    const net::URLRequestContext* context = request->context();
    net::TransportSecurityState* state = context->transport_security_state();
    if (state) {
      net::TransportSecurityState::DomainState domain_state;
      bool has_sni = net::SSLConfigService::IsSNIAvailable(
          context->ssl_config_service());
      if (state->GetDomainState(
              &domain_state, request->url().host(), has_sni) &&
          domain_state.ShouldMixedScriptingBeBlocked()) {
        filter->Send(new ChromeViewMsg_AddStrictSecurityHost(
            info->route_id(), request->url().host()));
      }
    }
  }

  // See if the response contains the X-Auto-Login header.  If so, this was
  // a request for a login page, and the server is allowing the browser to
  // suggest auto-login, if available.
  AutoLoginPrompter::ShowInfoBarIfPossible(request, info->child_id(),
                                           info->route_id());
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    net::URLRequest* request,
    content::ResourceResponse* response,
    ResourceMessageFilter* filter) {
  LoadTimingObserver::PopulateTimingInfo(request, response);
}
