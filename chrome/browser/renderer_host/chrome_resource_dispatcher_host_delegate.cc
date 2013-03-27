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
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/variations/variations_http_header_provider.h"
#include "chrome/browser/net/resource_prefetch_predictor_observer.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_url_request_user_data.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/auto_login_prompter.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/mime_types_handler.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "extensions/common/constants.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_mode_resource_throttle.h"
#endif

#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/repeated_field.h>
#else
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#endif

#if defined(OS_ANDROID)
#include "components/navigation_interception/intercept_navigation_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/merge_session_throttle.h"
// TODO(oshima): Enable this for other platforms.
#include "chrome/browser/renderer_host/offline_resource_throttle.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceDispatcherHostLoginDelegate;
using content::ResourceRequestInfo;
using extensions::Extension;

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

// The network stack returns actual connect times, while the renderer process
// expects times that the request was blocked in each phase of setting up
// a connection.  Due to preconnect and late binding, it is possible for a
// connection attempt to start before a request has been started, so this
// function is needed to convert times from the network stack to times the
// renderer process expects.
void FixupLoadTimingInfo(net::LoadTimingInfo* load_timing_info) {
  // If there are no times, do nothing.
  if (load_timing_info->request_start.is_null())
    return;

  // Starting the request and resolving the proxy are the only phases of the
  // request that occur before it blocks on starting a connection.
  base::TimeTicks block_on_connect_start = load_timing_info->request_start;
  if (!load_timing_info->proxy_resolve_end.is_null())
    block_on_connect_start = load_timing_info->proxy_resolve_end;

  net::LoadTimingInfo::ConnectTiming* connect_timing =
      &load_timing_info->connect_timing;
  if (!connect_timing->dns_start.is_null()) {
    DCHECK(!connect_timing->dns_end.is_null());
    if (connect_timing->dns_start < block_on_connect_start)
      connect_timing->dns_start = block_on_connect_start;
    if (connect_timing->dns_end < block_on_connect_start)
      connect_timing->dns_end = block_on_connect_start;
  }

  if (!connect_timing->connect_start.is_null()) {
    DCHECK(!connect_timing->connect_end.is_null());
    if (connect_timing->connect_start < block_on_connect_start)
      connect_timing->connect_start = block_on_connect_start;
    if (connect_timing->connect_end < block_on_connect_start)
      connect_timing->connect_end = block_on_connect_start;
  }

  if (!connect_timing->ssl_start.is_null()) {
    DCHECK(!connect_timing->ssl_end.is_null());
    if (connect_timing->ssl_start < block_on_connect_start)
      connect_timing->ssl_start = block_on_connect_start;
    if (connect_timing->ssl_end < block_on_connect_start)
      connect_timing->ssl_end = block_on_connect_start;
  }
}

// Goes through the extension's file browser handlers and checks if there is one
// that can handle the |mime_type|.
// |extension| must not be NULL.
bool ExtensionCanHandleMimeType(const Extension* extension,
                                const std::string& mime_type) {
  MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (!handler)
    return false;

  return handler->CanHandleMIMEType(mime_type);
}

// Retrieves Profile for a render view host specified by |render_process_id| and
// |render_view_id|.
Profile* GetProfile(int render_process_id, int render_view_id) {
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(render_view_host);
  if (!web_contents)
    return NULL;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return NULL;

  return Profile::FromBrowserContext(browser_context);
}

void SendExecuteMimeTypeHandlerEvent(scoped_ptr<content::StreamHandle> stream,
                                     int render_process_id,
                                     int render_view_id,
                                     const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  Profile* profile = GetProfile(render_process_id, render_view_id);
  if (!profile)
    return;

  extensions::StreamsPrivateAPI::Get(profile)->ExecuteMimeTypeHandler(
      extension_id, stream.Pass());
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

  // Abort any prerenders that spawn requests that use invalid HTTP methods
  // or invalid schemes.
  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id)) {
    if (!prerender::PrerenderManager::IsValidHttpMethod(method)) {
      prerender_tracker_->TryCancelOnIOThread(
          child_id, route_id, prerender::FINAL_STATUS_INVALID_HTTP_METHOD);
      return false;
    }
    if (!prerender::PrerenderManager::DoesURLHaveValidScheme(url)) {
      prerender_tracker_->TryCancelOnIOThread(
          child_id, route_id, prerender::FINAL_STATUS_UNSUPPORTED_SCHEME);
      return false;
    }
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
    request->SetPriority(net::IDLE);
  }

#if defined(OS_ANDROID)
  if (!is_prerendering && resource_type == ResourceType::MAIN_FRAME) {
    throttles->push_back(
        components::InterceptNavigationDelegate::CreateThrottleFor(request));
  }
#endif
#if defined(OS_CHROMEOS)
  if (resource_type == ResourceType::MAIN_FRAME) {
    // We check offline first, then check safe browsing so that we still can
    // block unsafe site after we remove offline page.
    throttles->push_back(new OfflineResourceThrottle(
        child_id, route_id, request, appcache_service));
    // Add interstitial page while merge session process (cookie
    // reconstruction from OAuth2 refresh token in ChromeOS login) is still in
    // progress while we are attempting to load a google property.
    throttles->push_back(new MergeSessionThrottle(
        child_id, route_id, request));
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

  if (prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id)) {
    prerender_tracker_->TryCancel(
        child_id, route_id, prerender::FINAL_STATUS_UNSUPPORTED_SCHEME);
    return false;
  }

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
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  // Insert safe browsing at the front of the list, so it gets to decide on
  // policies first.
  if (io_data->safe_browsing_enabled()->GetValue()) {
    bool is_subresource_request = resource_type != ResourceType::MAIN_FRAME;
    content::ResourceThrottle* throttle =
        SafeBrowsingResourceThrottleFactory::Create(request, child_id, route_id,
            is_subresource_request, safe_browsing_);
    if (throttle)
      throttles->push_back(throttle);
  }
#endif

#if defined(ENABLE_MANAGED_USERS)
  bool is_subresource_request = resource_type != ResourceType::MAIN_FRAME;
  throttles->push_back(new ManagedModeResourceThrottle(
        request, child_id, route_id, !is_subresource_request,
        io_data->managed_mode_url_filter()));
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

bool ChromeResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    content::ResourceContext* resource_context,
    const GURL& url,
    const std::string& mime_type,
    GURL* security_origin,
    std::string* target_id) {
#if !defined(OS_ANDROID)
  ProfileIOData* io_data =
      ProfileIOData::FromResourceContext(resource_context);
  bool profile_is_incognito = io_data->is_incognito();
  const scoped_refptr<const ExtensionInfoMap> extension_info_map(
      io_data->GetExtensionInfoMap());
  std::vector<std::string> whitelist = MimeTypesHandler::GetMIMETypeWhitelist();
  // Go through the white-listed extensions and try to use them to intercept
  // the URL request.
  for (size_t i = 0; i < whitelist.size(); ++i) {
    const char* extension_id = whitelist[i].c_str();
    const Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    // The white-listed extension may not be installed, so we have to NULL check
    // |extension|.
    if (!extension ||
        (profile_is_incognito &&
         !extension_info_map->IsIncognitoEnabled(extension_id))) {
      continue;
    }

    if (ExtensionCanHandleMimeType(extension, mime_type)) {
      *security_origin = Extension::GetBaseURLFromExtensionId(extension_id);
      *target_id = extension_id;
      return true;
    }
  }
#endif
  return false;
}

void ChromeResourceDispatcherHostDelegate::OnStreamCreated(
    content::ResourceContext* resource_context,
    int render_process_id,
    int render_view_id,
    const std::string& target_id,
    scoped_ptr<content::StreamHandle> stream) {
#if !defined(OS_ANDROID)
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SendExecuteMimeTypeHandlerEvent, base::Passed(&stream),
                 render_process_id, render_view_id,
                 target_id));
#endif
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response,
    IPC::Sender* sender) {
  // TODO(mmenke):  Figure out if LOAD_ENABLE_LOAD_TIMING is safe to remove.
  if (request->load_flags() & net::LOAD_ENABLE_LOAD_TIMING) {
    request->GetLoadTimingInfo(&response->head.load_timing);
    FixupLoadTimingInfo(&response->head.load_timing);
  }

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  if (request->url().SchemeIsSecure()) {
    const net::URLRequestContext* context = request->context();
    net::TransportSecurityState* state = context->transport_security_state();
    if (state) {
      net::TransportSecurityState::DomainState domain_state;
      bool has_sni = net::SSLConfigService::IsSNIAvailable(
          context->ssl_config_service());
      if (state->GetDomainState(request->url().host(), has_sni,
                                &domain_state) &&
          domain_state.ShouldUpgradeToSSL()) {
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

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, io_data,
                                              info->GetChildID(),
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

  if (io_data->resource_prefetch_predictor_observer())
    io_data->resource_prefetch_predictor_observer()->OnResponseStarted(request);

  prerender::URLRequestResponseStarted(request);
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  // TODO(mmenke):  Figure out if LOAD_ENABLE_LOAD_TIMING is safe to remove.
  if (request->load_flags() & net::LOAD_ENABLE_LOAD_TIMING) {
    request->GetLoadTimingInfo(&response->head.load_timing);
    FixupLoadTimingInfo(&response->head.load_timing);
  }

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, io_data,
                                              info->GetChildID(),
                                              info->GetRouteID());
  AppendChromeSyncGaiaHeader(request, resource_context);
#endif

  if (io_data->resource_prefetch_predictor_observer()) {
    io_data->resource_prefetch_predictor_observer()->OnRequestRedirected(
        redirect_url, request);
  }

  int child_id, route_id;
  if (!prerender::PrerenderManager::DoesURLHaveValidScheme(redirect_url) &&
      ResourceRequestInfo::ForRequest(request)->GetAssociatedRenderView(
          &child_id, &route_id) &&
      prerender_tracker_->IsPrerenderingOnIOThread(child_id, route_id)) {
    prerender_tracker_->TryCancel(
        child_id, route_id, prerender::FINAL_STATUS_UNSUPPORTED_SCHEME);
    request->Cancel();
  }
}
