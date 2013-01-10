// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_resource_throttle.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/net/load_timing_observer.h"
#include "chrome/browser/net/resource_prefetch_predictor_observer.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_url_request_user_data.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/auto_login_prompter.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
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
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/repeated_field.h>
#else
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#endif

#if defined(OS_ANDROID)
#include "content/components/navigation_interception/intercept_navigation_delegate.h"
#else
#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/file_browser_resource_throttle.h"
// TODO(oshima): Enable this for other platforms.
#include "chrome/browser/renderer_host/offline_resource_throttle.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceDispatcherHostLoginDelegate;
using content::ResourceRequestInfo;

namespace {

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
      user_script_listener_(new extensions::UserScriptListener()),
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
    appcache::AppCacheService* appcache_service,
    ResourceType::Type resource_type,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request,
    ScopedVector<content::ResourceThrottle>* throttles) {
  if (is_continuation_of_transferred_request)
    ChromeURLRequestUserData::Delete(request);

  ChromeURLRequestUserData* user_data =
      ChromeURLRequestUserData::Create(request);
  bool is_prerendering = prerender_tracker_->IsPrerenderingOnIOThread(
      child_id, route_id);
  if (is_prerendering) {
    user_data->set_is_prerender(true);
    request->set_priority(net::IDLE);
  }

#if defined(OS_ANDROID)
  if (!is_prerendering && resource_type == ResourceType::MAIN_FRAME) {
    throttles->push_back(
        content::InterceptNavigationDelegate::CreateThrottleFor(
            request));
  }
#endif
#if defined(OS_CHROMEOS)
  if (resource_type == ResourceType::MAIN_FRAME) {
    // We check offline first, then check safe browsing so that we still can
    // block unsafe site after we remove offline page.
    throttles->push_back(new OfflineResourceThrottle(
        child_id, route_id, request, appcache_service));
  }
#endif

  // Don't attempt to append headers to requests that have already started.
  // TODO(stevet): Remove this once the request ordering issues are resolved
  // in crbug.com/128048.
  if (!request->is_pending()) {
    net::HttpRequestHeaders headers;
    headers.CopyFrom(request->extra_request_headers());
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(
        resource_context);
    bool incognito = io_data->is_incognito();
    chrome_variations::VariationsHttpHeaderProvider::GetInstance()->
        AppendHeaders(request->url(),
                      incognito,
                      !incognito && io_data->GetMetricsEnabledStateOnIOThread(),
                      &headers);
    request->SetExtraRequestHeaders(headers);
  }

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  AppendChromeSyncGaiaHeader(request, resource_context);
#endif

  AppendStandardResourceThrottles(request,
                                  resource_context,
                                  child_id,
                                  route_id,
                                  resource_type,
                                  throttles);

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (io_data->resource_prefetch_predictor_observer()) {
    io_data->resource_prefetch_predictor_observer()->OnRequestStarted(
        request, resource_type, child_id, route_id);
  }
}

void ChromeResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    int child_id,
    int route_id,
    int request_id,
    bool is_content_initiated,
    bool must_download,
    ScopedVector<content::ResourceThrottle>* throttles) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyDownloadInitiatedOnUI, child_id, route_id));

  // If it's from the web, we don't trust it, so we push the throttle on.
  if (is_content_initiated) {
#if defined(OS_CHROMEOS)
    if (!must_download) {
      ProfileIOData* io_data =
          ProfileIOData::FromResourceContext(resource_context);
      throttles->push_back(FileBrowserResourceThrottle::Create(
          child_id, route_id, request, io_data->is_incognito(),
          io_data->GetExtensionInfoMap()));
    }
#endif  // defined(OS_CHROMEOS)

    throttles->push_back(new DownloadResourceThrottle(
        download_request_limiter_, child_id, route_id, request_id,
        request->method()));
  }

  // If this isn't a new request, we've seen this before and added the standard
  //  resource throttles already so no need to add it again.
  if (!request->is_pending()) {
    AppendStandardResourceThrottles(request,
                                    resource_context,
                                    child_id,
                                    route_id,
                                    ResourceType::MAIN_FRAME,
                                    throttles);
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
  return CreateLoginPrompt(auth_info, request);
}

bool ChromeResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url, int child_id, int route_id) {
#if defined(OS_ANDROID)
  // Android use a resource throttle to handle external as well as internal
  // protocols.
  return false;
#else
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ExternalProtocolHandler::LaunchUrl, url, child_id, route_id));
  return true;
#endif
}

void ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    int child_id,
    int route_id,
    ResourceType::Type resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  // Insert safe browsing at the front of the list, so it gets to decide on
  // policies first.
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (io_data->safe_browsing_enabled()->GetValue()) {
    bool is_subresource_request = resource_type != ResourceType::MAIN_FRAME;
    content::ResourceThrottle* throttle =
        SafeBrowsingResourceThrottleFactory::Create(request, child_id, route_id,
            is_subresource_request, safe_browsing_);
    if (throttle)
      throttles->push_back(throttle);
  }
#endif

#if !defined(OS_ANDROID)
  bool is_subresource_request = resource_type != ResourceType::MAIN_FRAME;
  throttles->push_back(new ManagedModeResourceThrottle(
        request, child_id, route_id, !is_subresource_request));
#endif

  content::ResourceThrottle* throttle =
      user_script_listener_->CreateResourceThrottle(request->url(),
                                                    resource_type);
  if (throttle)
    throttles->push_back(throttle);
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void ChromeResourceDispatcherHostDelegate::AppendChromeSyncGaiaHeader(
    net::URLRequest* request,
    content::ResourceContext* resource_context) {
  static const char kAllowChromeSignIn[] = "Allow-Chrome-SignIn";

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  OneClickSigninHelper::Offer offer =
      OneClickSigninHelper::CanOfferOnIOThread(request, io_data);
  switch (offer) {
    case OneClickSigninHelper::CAN_OFFER:
      request->SetExtraRequestHeaderByName(kAllowChromeSignIn, "1", false);
      break;
    case OneClickSigninHelper::DONT_OFFER:
      request->RemoveRequestHeaderByName(kAllowChromeSignIn);
      break;
    case OneClickSigninHelper::IGNORE_REQUEST:
      break;
  }
}
#endif

bool ChromeResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url, const std::string& mime_type) {
  // Special-case user scripts to get downloaded instead of viewed.
  return extensions::UserScript::IsURLUserScript(url, mime_type);
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response,
    IPC::Sender* sender) {
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

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, info->GetChildID(),
                                              info->GetRouteID());
#endif

  // Build in additional protection for the chrome web store origin.
  GURL webstore_url(extension_urls::GetWebstoreLaunchURL());
  if (request->url().DomainIs(webstore_url.host().c_str())) {
    net::HttpResponseHeaders* response_headers = request->response_headers();
    if (!response_headers->HasHeaderValue("x-frame-options", "deny") &&
        !response_headers->HasHeaderValue("x-frame-options", "sameorigin")) {
      response_headers->RemoveHeader("x-frame-options");
      response_headers->AddHeader("x-frame-options: sameorigin");
    }
  }

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (io_data->resource_prefetch_predictor_observer())
    io_data->resource_prefetch_predictor_observer()->OnResponseStarted(request);

  prerender::URLRequestResponseStarted(request);
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  LoadTimingObserver::PopulateTimingInfo(request, response);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  AppendChromeSyncGaiaHeader(request, resource_context);

  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, info->GetChildID(),
                                              info->GetRouteID());
#endif

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (io_data->resource_prefetch_predictor_observer()) {
    io_data->resource_prefetch_predictor_observer()->OnRequestRedirected(
        redirect_url, request);
  }
}
