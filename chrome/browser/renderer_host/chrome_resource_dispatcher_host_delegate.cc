// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_resource_throttle.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/net/load_timing_observer.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_url_request_user_data.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle.h"
#include "chrome/browser/renderer_host/transfer_navigation_resource_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/auto_login_prompter.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/load_flags.h"
#include "net/base/ssl_config_service.h"
#include "net/url_request/url_request.h"

// TODO(oshima): Enable this for other platforms.
#if defined(OS_CHROMEOS)
#include "chrome/browser/renderer_host/offline_resource_throttle.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceDispatcherHostLoginDelegate;
using content::ResourceRequestInfo;

namespace {

void AddPrerenderOnUI(
    int render_process_id, int render_view_id,
    const GURL& url, const content::Referrer& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prerender::PrerenderManager* prerender_manager =
      prerender::FindPrerenderManagerUsingRenderProcessId(render_process_id);
  if (!prerender_manager)
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
    prerender::PrerenderTracker* prerender_tracker)
    : download_request_limiter_(g_browser_process->download_request_limiter()),
      safe_browsing_(g_browser_process->safe_browsing_service()),
      user_script_listener_(new UserScriptListener()),
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
    content::ResourceContext* resource_context,
    const content::Referrer& referrer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Handle a PREFETCH resource type. If prefetch is disabled, squelch the
  // request.  Otherwise, do a normal request to warm the cache.
  if (resource_type == ResourceType::PREFETCH) {
    // All PREFETCH requests should be GETs, but be defensive about it.
    if (method != "GET")
      return false;

    // If prefetch is disabled, kill the request.
    if (!prerender::PrerenderManager::IsPrefetchEnabled())
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

void ChromeResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType::Type resource_type,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request,
    ScopedVector<content::ResourceThrottle>* throttles) {
  if (is_continuation_of_transferred_request)
    ChromeURLRequestUserData::Delete(request);

  ChromeURLRequestUserData* user_data =
      ChromeURLRequestUserData::Create(request);
  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id)) {
    user_data->set_is_prerender(true);
    request->set_priority(net::IDLE);
  }

  if (resource_type == ResourceType::MAIN_FRAME) {
    throttles->push_back(new TransferNavigationResourceThrottle(request));

#if defined(OS_CHROMEOS)
    // We check offline first, then check safe browsing so that we still can
    // block unsafe site after we remove offline page.
    throttles->push_back(new OfflineResourceThrottle(
        child_id, route_id, request, resource_context));
#endif
  }

  AppendChromeMetricsHeaders(request, resource_context, resource_type);

  AppendStandardResourceThrottles(request,
                                  resource_context,
                                  child_id,
                                  route_id,
                                  resource_type,
                                  throttles);
}

void ChromeResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    int child_id,
    int route_id,
    int request_id,
    bool is_new_request,
    ScopedVector<content::ResourceThrottle>* throttles) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyDownloadInitiatedOnUI, child_id, route_id));

  // If this isn't a new request, we've seen this before and added the safe
  // browsing resource throttle already so no need to add it again. This code
  // path is only hit for requests initiated through the browser, and not the
  // web, so no need to add the download throttle.
  if (is_new_request) {
    AppendStandardResourceThrottles(request,
                                    resource_context,
                                    child_id,
                                    route_id,
                                    ResourceType::MAIN_FRAME,
                                    throttles);
  } else {
    throttles->push_back(new DownloadResourceThrottle(
        download_request_limiter_, child_id, route_id, request_id,
        request->method()));
  }
}

bool ChromeResourceDispatcherHostDelegate::AcceptSSLClientCertificateRequest(
    net::URLRequest* request, net::SSLCertRequestInfo* cert_request_info) {
  if (request->load_flags() & net::LOAD_PREFETCH)
    return false;

  ChromeURLRequestUserData* user_data = ChromeURLRequestUserData::Get(request);
  if (user_data && user_data->is_prerender()) {
    int child_id, route_id;
    if (ResourceRequestInfo::ForRequest(request)->GetAssociatedRenderView(
            &child_id, &route_id)) {
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
  if (!ResourceRequestInfo::ForRequest(request)->GetAssociatedRenderView(
          &child_id, &route_id)) {
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
  // For instant, return a NULL delegate. Auth navigations don't commit the load
  // (the load remains pending) until the user cancels or succeeds in
  // authorizing. Since we don't allow merging of WebContents with pending loads
  // we disallow auth dialogs from showing during instant. Returning NULL does
  // that.
  // TODO: see if we can handle this case more robustly.
  if (request->extra_request_headers().GetHeader(
          InstantLoader::kInstantHeader, &instant_header_value) &&
      instant_header_value == InstantLoader::kInstantHeaderValue)
    return NULL;
  return CreateLoginPrompt(auth_info, request);
}

void ChromeResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url, int child_id, int route_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalProtocolHandler::LaunchUrl, url, child_id, route_id));
}

void ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
    const net::URLRequest* request,
    content::ResourceContext* resource_context,
    int child_id,
    int route_id,
    ResourceType::Type resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
#if defined(ENABLE_SAFE_BROWSING)
  // Insert safe browsing at the front of the list, so it gets to decide on
  // policies first.
  bool is_subresource_request = resource_type != ResourceType::MAIN_FRAME;
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (io_data->safe_browsing_enabled()->GetValue()) {
    throttles->push_back(SafeBrowsingResourceThrottle::Create(
        request, child_id, route_id, is_subresource_request, safe_browsing_));
  }
#endif

  content::ResourceThrottle* throttle =
      user_script_listener_->CreateResourceThrottle(request->url(),
                                                    resource_type);
  if (throttle)
    throttles->push_back(throttle);
}

void ChromeResourceDispatcherHostDelegate::AppendChromeMetricsHeaders(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType::Type resource_type) {
  // Note our criteria for attaching Chrome experiment headers:
  // 1. We only transmit to *.google.<TLD> domains. NOTE that this use of
  //    google_util helpers to check this does not guarantee that the URL is
  //    Google-owned, only that it is of the form *.google.<TLD>. In the future
  //    we may choose to reinforce this check.
  // 2. We only transmit for non-Incognito profiles.
  // 3. For the X-Chrome-UMA-Enabled bit, we only set it if UMA is in fact
  //    enabled for this install of Chrome.
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (google_util::IsGoogleDomainUrl(request->url().spec(),
                                     google_util::ALLOW_SUBDOMAIN) &&
      !io_data->is_incognito() && io_data->GetMetricsEnabledStateOnIOThread()) {
    request->SetExtraRequestHeaderByName("X-Chrome-UMA-Enabled",
                                         "1",
                                         false);
  }
}

bool ChromeResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url, const std::string& mime_type) {
  // Special-case user scripts to get downloaded instead of viewed.
  return UserScript::IsURLUserScript(url, mime_type);
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceResponse* response,
    IPC::Message::Sender* sender) {
  LoadTimingObserver::PopulateTimingInfo(request, response);

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  if (request->url().SchemeIsSecure()) {
    const net::URLRequestContext* context = request->context();
    net::TransportSecurityState* state = context->transport_security_state();
    if (state) {
      net::TransportSecurityState::DomainState domain_state;
      bool has_sni = net::SSLConfigService::IsSNIAvailable(
          context->ssl_config_service());
      if (state->GetDomainState(
              request->url().host(), has_sni, &domain_state)) {
        sender->Send(new ChromeViewMsg_AddStrictSecurityHost(
            info->GetRouteID(), request->url().host()));
      }
    }
  }

  // See if the response contains the X-Auto-Login header.  If so, this was
  // a request for a login page, and the server is allowing the browser to
  // suggest auto-login, if available.
  AutoLoginPrompter::ShowInfoBarIfPossible(request, info->GetChildID(),
                                           info->GetRouteID());
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    net::URLRequest* request,
    content::ResourceResponse* response) {
  LoadTimingObserver::PopulateTimingInfo(request, response);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, info->GetChildID(),
                                              info->GetRouteID());
#endif
}
