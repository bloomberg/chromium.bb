// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/guid.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/component_updater_resource_throttle.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_resource_throttle.h"
#include "chrome/browser/prefetch/prefetch.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_pending_swap_throttle.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_throttle_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/app_url_redirector.h"
#include "chrome/browser/apps/ephemeral_app_throttle.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/guest_view/web_view/web_view_renderer_state.h"
#include "chrome/common/extensions/manifest_handlers/mime_types_handler.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/user_script.h"
#endif

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"
#endif

#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/repeated_field.h>
#else
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/intercept_download_resource_throttle.h"
#include "chrome/browser/ui/android/infobars/auto_login_prompter.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/signin/merge_session_throttle.h"
// TODO(oshima): Enable this for other platforms.
#include "chrome/browser/renderer_host/offline_resource_throttle.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceDispatcherHostLoginDelegate;
using content::ResourceRequestInfo;
using content::ResourceType;

#if defined(ENABLE_EXTENSIONS)
using extensions::Extension;
using extensions::StreamsPrivateAPI;
#endif

#if defined(OS_ANDROID)
using navigation_interception::InterceptNavigationDelegate;
#endif

namespace {

ExternalProtocolHandler::Delegate* g_external_protocol_handler_delegate = NULL;

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

prerender::PrerenderManager* GetPrerenderManager(int render_process_id,
                                                 int render_view_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!web_contents)
    return NULL;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return NULL;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return NULL;

  return prerender::PrerenderManagerFactory::GetForProfile(profile);
}

void UpdatePrerenderNetworkBytesCallback(int render_process_id,
                                         int render_view_id,
                                         int64 bytes) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  // PrerenderContents::FromWebContents handles the NULL case.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);

  if (prerender_contents)
    prerender_contents->AddNetworkBytes(bytes);

  prerender::PrerenderManager* prerender_manager =
      GetPrerenderManager(render_process_id, render_view_id);
  if (prerender_manager)
    prerender_manager->AddProfileNetworkBytesIfEnabled(bytes);
}

#if defined(ENABLE_EXTENSIONS)
void SendExecuteMimeTypeHandlerEvent(scoped_ptr<content::StreamHandle> stream,
                                     int64 expected_content_size,
                                     int render_process_id,
                                     int render_view_id,
                                     const std::string& extension_id,
                                     const std::string& view_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!web_contents)
    return;

  // If the request was for a prerender, abort the prerender and do not
  // continue.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_DOWNLOAD);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  StreamsPrivateAPI* streams_private = StreamsPrivateAPI::Get(profile);
  if (!streams_private)
    return;
  streams_private->ExecuteMimeTypeHandler(
      extension_id, web_contents, stream.Pass(), view_id,
      expected_content_size);
}
#endif  // !defined(ENABLE_EXTENSIONS)

#if !defined(OS_ANDROID)
void LaunchURL(const GURL& url, int render_process_id, int render_view_id) {
  // If there is no longer a WebContents, the request may have raced with tab
  // closing. Don't fire the external request. (It may have been a prerender.)
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!web_contents)
    return;

  // Do not launch external requests attached to unswapped prerenders.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_UNSUPPORTED_SCHEME);
    prerender::ReportPrerenderExternalURL();
    return;
  }

  ExternalProtocolHandler::LaunchUrlWithDelegate(
      url,
      render_process_id,
      render_view_id,
      g_external_protocol_handler_delegate);
}
#endif  // !defined(OS_ANDROID)

void AppendComponentUpdaterThrottles(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
  const char* crx_id = NULL;
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (!cus)
    return;
  // Check for PNaCl pexe request.
  if (resource_type == content::RESOURCE_TYPE_OBJECT) {
    const net::HttpRequestHeaders& headers = request->extra_request_headers();
    std::string accept_headers;
    if (headers.GetHeader("Accept", &accept_headers)) {
      if (accept_headers.find("application/x-pnacl") != std::string::npos &&
          pnacl::NeedsOnDemandUpdate())
        crx_id = "hnimpnehoodheedghdeeijklkeaacbdc";
    }
  }

  if (crx_id) {
    // We got a component we need to install, so throttle the resource
    // until the component is installed.
    throttles->push_back(
        component_updater::GetOnDemandResourceThrottle(cus, crx_id));
  }
}

}  // end namespace

ChromeResourceDispatcherHostDelegate::ChromeResourceDispatcherHostDelegate(
    prerender::PrerenderTracker* prerender_tracker)
    : download_request_limiter_(g_browser_process->download_request_limiter()),
      safe_browsing_(g_browser_process->safe_browsing_service()),
#if defined(ENABLE_EXTENSIONS)
      user_script_listener_(new extensions::UserScriptListener()),
#endif
      prerender_tracker_(prerender_tracker) {
}

ChromeResourceDispatcherHostDelegate::~ChromeResourceDispatcherHostDelegate() {
#if defined(ENABLE_EXTENSIONS)
  CHECK(stream_target_info_.empty());
#endif
}

bool ChromeResourceDispatcherHostDelegate::ShouldBeginRequest(
    int child_id,
    int route_id,
    const std::string& method,
    const GURL& url,
    ResourceType resource_type,
    content::ResourceContext* resource_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Handle a PREFETCH resource type. If prefetch is disabled, squelch the
  // request.  Otherwise, do a normal request to warm the cache.
  if (resource_type == content::RESOURCE_TYPE_PREFETCH) {
    // All PREFETCH requests should be GETs, but be defensive about it.
    if (method != "GET")
      return false;

    // If prefetch is disabled, kill the request.
    if (!prefetch::IsPrefetchEnabled(resource_context))
      return false;
  }

  return true;
}

void ChromeResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    ResourceType resource_type,
    int child_id,
    int route_id,
    ScopedVector<content::ResourceThrottle>* throttles) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  bool is_prerendering =
      info->GetVisibilityState() == blink::WebPageVisibilityStatePrerender;
  if (is_prerendering) {
    // Requests with the IGNORE_LIMITS flag set (i.e., sync XHRs)
    // should remain at MAXIMUM_PRIORITY.
    if (request->load_flags() & net::LOAD_IGNORE_LIMITS) {
      DCHECK_EQ(request->priority(), net::MAXIMUM_PRIORITY);
    } else {
      request->SetPriority(net::IDLE);
    }
  }

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(
      resource_context);

#if defined(OS_ANDROID)
  // TODO(davidben): This is insufficient to integrate with prerender properly.
  // https://crbug.com/370595
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME && !is_prerendering) {
    throttles->push_back(
        InterceptNavigationDelegate::CreateThrottleFor(request));
  }
#else
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // Redirect some navigations to apps that have registered matching URL
    // handlers ('url_handlers' in the manifest).
    content::ResourceThrottle* url_to_app_throttle =
        AppUrlRedirector::MaybeCreateThrottleFor(request, io_data);
    if (url_to_app_throttle)
      throttles->push_back(url_to_app_throttle);

    if (!is_prerendering) {
      // Experimental: Launch ephemeral apps from search results.
      content::ResourceThrottle* ephemeral_app_throttle =
          EphemeralAppThrottle::MaybeCreateThrottleForLaunch(
              request, io_data);
      if (ephemeral_app_throttle)
        throttles->push_back(ephemeral_app_throttle);
    }
  }
#endif

#if defined(OS_CHROMEOS)
  // Check if we need to add offline throttle. This should be done only
  // for main frames.
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // We check offline first, then check safe browsing so that we still can
    // block unsafe site after we remove offline page.
    throttles->push_back(new OfflineResourceThrottle(request,
                                                     appcache_service));
  }

  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of main frames and XHR request.
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
      resource_type == content::RESOURCE_TYPE_XHR) {
    // Add interstitial page while merge session process (cookie
    // reconstruction from OAuth2 refresh token in ChromeOS login) is still in
    // progress while we are attempting to load a google property.
    if (!MergeSessionThrottle::AreAllSessionMergedAlready() &&
        request->url().SchemeIsHTTPOrHTTPS()) {
      throttles->push_back(new MergeSessionThrottle(request, resource_type));
    }
  }
#endif

  // Don't attempt to append headers to requests that have already started.
  // TODO(stevet): Remove this once the request ordering issues are resolved
  // in crbug.com/128048.
  if (!request->is_pending()) {
    net::HttpRequestHeaders headers;
    headers.CopyFrom(request->extra_request_headers());
    bool is_off_the_record = io_data->IsOffTheRecord();
    variations::VariationsHttpHeaderProvider::GetInstance()->
        AppendHeaders(request->url(),
                      is_off_the_record,
                      !is_off_the_record &&
                          io_data->GetMetricsEnabledStateOnIOThread(),
                      &headers);
    request->SetExtraRequestHeaders(headers);
  }

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  AppendChromeSyncGaiaHeader(request, resource_context);
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
  if (io_data->policy_header_helper())
    io_data->policy_header_helper()->AddPolicyHeaders(request->url(), request);
#endif

  signin::AppendMirrorRequestHeaderIfPossible(
      request, GURL() /* redirect_url */,
      io_data, info->GetChildID(), info->GetRouteID());

  AppendStandardResourceThrottles(request,
                                  resource_context,
                                  resource_type,
                                  throttles);
  if (!is_prerendering) {
    AppendComponentUpdaterThrottles(request,
                                    resource_context,
                                    resource_type,
                                    throttles);
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
    throttles->push_back(
        new DownloadResourceThrottle(download_request_limiter_.get(),
                                     child_id,
                                     route_id,
                                     request->url(),
                                     request->method()));
#if defined(OS_ANDROID)
    throttles->push_back(
        new chrome::InterceptDownloadResourceThrottle(
            request, child_id, route_id, request_id));
#endif
  }

  // If this isn't a new request, we've seen this before and added the standard
  //  resource throttles already so no need to add it again.
  if (!request->is_pending()) {
    AppendStandardResourceThrottles(request,
                                    resource_context,
                                    content::RESOURCE_TYPE_MAIN_FRAME,
                                    throttles);
  }
}

ResourceDispatcherHostLoginDelegate*
    ChromeResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info, net::URLRequest* request) {
  return CreateLoginPrompt(auth_info, request);
}

bool ChromeResourceDispatcherHostDelegate::HandleExternalProtocol(
    const GURL& url,
    int child_id,
    int route_id) {
#if defined(OS_ANDROID)
  // Android use a resource throttle to handle external as well as internal
  // protocols.
  return false;
#else

#if defined(ENABLE_EXTENSIONS)
  if (WebViewRendererState::GetInstance()->IsGuest(child_id))
    return false;

#endif  // defined(ENABLE_EXTENSIONS)

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&LaunchURL, url, child_id, route_id));
  return true;
#endif
}

void ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  // Insert safe browsing at the front of the list, so it gets to decide on
  // policies first.
  if (io_data->safe_browsing_enabled()->GetValue()
#if defined(OS_ANDROID)
      || io_data->IsDataReductionProxyEnabled()
#endif
  ) {
    bool is_subresource_request =
        resource_type != content::RESOURCE_TYPE_MAIN_FRAME;
    content::ResourceThrottle* throttle =
        SafeBrowsingResourceThrottleFactory::Create(request,
                                                    resource_context,
                                                    is_subresource_request,
                                                    safe_browsing_.get());
    if (throttle)
      throttles->push_back(throttle);
  }
#endif

#if defined(ENABLE_MANAGED_USERS)
  bool is_subresource_request =
      resource_type != content::RESOURCE_TYPE_MAIN_FRAME;
  throttles->push_back(new SupervisedUserResourceThrottle(
        request, !is_subresource_request,
        io_data->supervised_user_url_filter()));
#endif

#if defined(ENABLE_EXTENSIONS)
  content::ResourceThrottle* throttle =
      user_script_listener_->CreateResourceThrottle(request->url(),
                                                    resource_type);
  if (throttle)
    throttles->push_back(throttle);
#endif

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info->GetVisibilityState() == blink::WebPageVisibilityStatePrerender) {
    throttles->push_back(new prerender::PrerenderResourceThrottle(request));
  }
  if (prerender_tracker_->IsPendingSwapRequestOnIOThread(
          info->GetChildID(), info->GetRenderFrameID(), request->url())) {
    throttles->push_back(new prerender::PrerenderPendingSwapThrottle(
        request, prerender_tracker_));
  }
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
#if defined(ENABLE_EXTENSIONS)
  // Special-case user scripts to get downloaded instead of viewed.
  return extensions::UserScript::IsURLUserScript(url, mime_type);
#else
  return false;
#endif
}

bool ChromeResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
#if defined(ENABLE_EXTENSIONS)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  ProfileIOData* io_data =
      ProfileIOData::FromResourceContext(info->GetContext());
  bool profile_is_off_the_record = io_data->IsOffTheRecord();
  const scoped_refptr<const extensions::InfoMap> extension_info_map(
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
        (profile_is_off_the_record &&
         !extension_info_map->IsIncognitoEnabled(extension_id))) {
      continue;
    }

    MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
    if (handler && handler->CanHandleMIMEType(mime_type)) {
      StreamTargetInfo target_info;
      *origin = Extension::GetBaseURLFromExtensionId(extension_id);
      target_info.extension_id = extension_id;
      if (!handler->handler_url().empty()) {
        target_info.view_id = base::GenerateGUID();
        *payload = origin->spec() + handler->handler_url() +
            "?id=" + target_info.view_id;
      }
      stream_target_info_[request] = target_info;
      return true;
    }
  }
#endif
  return false;
}

void ChromeResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    scoped_ptr<content::StreamHandle> stream) {
#if defined(ENABLE_EXTENSIONS)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  std::map<net::URLRequest*, StreamTargetInfo>::iterator ix =
      stream_target_info_.find(request);
  CHECK(ix != stream_target_info_.end());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SendExecuteMimeTypeHandlerEvent, base::Passed(&stream),
                 request->GetExpectedContentSize(),
                 info->GetChildID(), info->GetRouteID(),
                 ix->second.extension_id, ix->second.view_id));
  stream_target_info_.erase(request);
#endif
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response,
    IPC::Sender* sender) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

#if defined(OS_ANDROID)
  // See if the response contains the X-Auto-Login header.  If so, this was
  // a request for a login page, and the server is allowing the browser to
  // suggest auto-login, if available.
  AutoLoginPrompter::ShowInfoBarIfPossible(request, info->GetChildID(),
                                           info->GetRouteID());
#endif

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, io_data,
                                              info->GetChildID(),
                                              info->GetRouteID());
#endif

  // See if the response contains the X-Chrome-Manage-Accounts header. If so
  // show the profile avatar bubble so that user can complete signin/out action
  // the native UI.
  signin::ProcessMirrorResponseHeaderIfExists(request, io_data,
                                              info->GetChildID(),
                                              info->GetRouteID());

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

  // Ignores x-frame-options for the chrome signin UI.
  const std::string request_spec(
      request->first_party_for_cookies().GetOrigin().spec());
#if defined(OS_CHROMEOS)
  if (request_spec == chrome::kChromeUIOobeURL ||
      request_spec == chrome::kChromeUIChromeSigninURL) {
#else
  if (request_spec == chrome::kChromeUIChromeSigninURL) {
#endif
    net::HttpResponseHeaders* response_headers = request->response_headers();
    if (response_headers && response_headers->HasHeader("x-frame-options"))
      response_headers->RemoveHeader("x-frame-options");
  }

  prerender::URLRequestResponseStarted(request);
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // See if the response contains the Google-Accounts-SignIn header.  If so,
  // then the user has just finished signing in, and the server is allowing the
  // browser to suggest connecting the user's profile to the account.
  OneClickSigninHelper::ShowInfoBarIfPossible(request, io_data,
                                              info->GetChildID(),
                                              info->GetRouteID());
  AppendChromeSyncGaiaHeader(request, resource_context);
#endif

  // In the Mirror world, Chrome should append a X-Chrome-Connected header to
  // all Gaia requests from a connected profile so Gaia could return a 204
  // response and let Chrome handle the action with native UI. The only
  // exception is requests from gaia webview, since the native profile
  // management UI is built on top of it.
  signin::AppendMirrorRequestHeaderIfPossible(request, redirect_url, io_data,
      info->GetChildID(), info->GetRouteID());

#if defined(ENABLE_CONFIGURATION_POLICY)
  if (io_data->policy_header_helper())
    io_data->policy_header_helper()->AddPolicyHeaders(redirect_url, request);
#endif
}

// Notification that a request has completed.
void ChromeResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {
  // Jump on the UI thread and inform the prerender about the bytes.
  const ResourceRequestInfo* info =
      ResourceRequestInfo::ForRequest(url_request);
  if (url_request && !url_request->was_cached()) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&UpdatePrerenderNetworkBytesCallback,
                                       info->GetChildID(),
                                       info->GetRouteID(),
                                       url_request->GetTotalReceivedBytes()));
  }
}

// static
void ChromeResourceDispatcherHostDelegate::
    SetExternalProtocolHandlerDelegateForTesting(
    ExternalProtocolHandler::Delegate* delegate) {
  g_external_protocol_handler_delegate = delegate;
}
