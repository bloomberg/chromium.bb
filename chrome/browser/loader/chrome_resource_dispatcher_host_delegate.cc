// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/component_updater_resource_throttle.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_resource_throttle.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/loader/predictor_resource_throttle.h"
#include "chrome/browser/loader/safe_browsing_resource_throttle.h"
#include "chrome/browser/mod_pagespeed/mod_pagespeed_metrics.h"
#include "chrome/browser/net/resource_prefetch_predictor_observer.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#include "components/rappor/rappor_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/stream_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_response.h"
#include "extensions/features/features.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"

#if !defined(DISABLE_NACL)
#include "chrome/browser/component_updater/pnacl_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/app_url_redirector.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "extensions/browser/extension_throttle_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/user_script.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_resource_throttle.h"
#endif

#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/repeated_field.h>
#else
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/loader/data_reduction_proxy_resource_throttle_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/signin/merge_session_resource_throttle.h"
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceDispatcherHostLoginDelegate;
using content::ResourceRequestInfo;
using content::ResourceType;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
using extensions::StreamsPrivateAPI;
#endif

#if defined(OS_ANDROID)
using navigation_interception::InterceptNavigationDelegate;
#endif

namespace {

ExternalProtocolHandler::Delegate* g_external_protocol_handler_delegate = NULL;

void NotifyDownloadInitiatedOnUI(
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter) {
  content::WebContents* web_contents = wc_getter.Run();
  if (!web_contents)
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_DOWNLOAD_INITIATED,
      content::Source<content::WebContents>(web_contents),
      content::NotificationService::NoDetails());
}

prerender::PrerenderManager* GetPrerenderManager(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return NULL;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return NULL;

  return prerender::PrerenderManagerFactory::GetForBrowserContext(
      browser_context);
}

void UpdatePrerenderNetworkBytesCallback(content::WebContents* web_contents,
                                         int64_t bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // PrerenderContents::FromWebContents handles the NULL case.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);

  if (prerender_contents)
    prerender_contents->AddNetworkBytes(bytes);

  prerender::PrerenderManager* prerender_manager =
      GetPrerenderManager(web_contents);
  if (prerender_manager)
    prerender_manager->AddProfileNetworkBytesIfEnabled(bytes);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void SendExecuteMimeTypeHandlerEvent(
    std::unique_ptr<content::StreamInfo> stream,
    int64_t expected_content_size,
    const std::string& extension_id,
    const std::string& view_id,
    bool embedded,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents = nullptr;
  if (frame_tree_node_id != -1) {
    web_contents =
        content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  } else {
    web_contents = content::WebContents::FromRenderFrameHost(
        content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  }
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
      extension_id, std::move(stream), view_id, expected_content_size, embedded,
      frame_tree_node_id, render_process_id, render_frame_id);
}
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)

void LaunchURL(
    const GURL& url,
    int render_process_id,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    bool is_whitelisted) {
  // If there is no longer a WebContents, the request may have raced with tab
  // closing. Don't fire the external request. (It may have been a prerender.)
  content::WebContents* web_contents = web_contents_getter.Run();
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

  // If the URL is in whitelist, we launch it without asking the user and
  // without any additional security checks. Since the URL is whitelisted,
  // we assume it can be executed.
  // TODO(davidsac): External protocol handling needs to be
  // fixed for OOPIFs.  See https://crbug.com/668289.
  if (is_whitelisted) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
        url, render_process_id,
        web_contents->GetRenderViewHost()->GetRoutingID());
  } else {
    ExternalProtocolHandler::LaunchUrlWithDelegate(
        url, render_process_id,
        web_contents->GetRenderViewHost()->GetRoutingID(), page_transition,
        has_user_gesture, g_external_protocol_handler_delegate);
  }
}

#if !defined(DISABLE_NACL)
void AppendComponentUpdaterThrottles(
    net::URLRequest* request,
    const ResourceRequestInfo& info,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
  bool is_prerendering =
      info.GetVisibilityState() == blink::WebPageVisibilityStatePrerender;
  if (is_prerendering)
    return;

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
#endif  // !defined(DISABLE_NACL)

// This function is called in NotifyUIThreadOfRequestComplete to log metrics
// about main frame resources.
void LogMainFrameMetricsOnUIThread(const GURL& url,
                                   int net_error,
                                   base::TimeDelta request_loading_time,
                                   content::WebContents* web_contents) {
  DCHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // The rest of the function is only concerned about NTP metrics.
  if (!profile || !search::IsNTPURL(url, profile))
    return;

  // A http/s scheme implies that the new tab page is remote. The local NTP is
  // served from chrome-search://. To segment out remote NTPs, use the
  // TemplateURLService to find the default search engine. Note that if the
  // default search engine does not have a remote NTP, then the local NTP will
  // be shown. As of April 2016, only Bing and Google have remote NTPs.
  if (url.SchemeIsHTTPOrHTTPS()) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    if (!template_url_service)
      return;
    TemplateURL* default_provider =
        template_url_service->GetDefaultSearchProvider();
    if (!default_provider)
      return;
    if (default_provider->GetEngineType(
            template_url_service->search_terms_data()) ==
        SearchEngineType::SEARCH_ENGINE_GOOGLE) {
      if (net_error == net::OK) {
        UMA_HISTOGRAM_LONG_TIMES("Net.NTP.Google.RequestTime2.Success",
                                 request_loading_time);
      } else if (net_error == net::ERR_ABORTED) {
        UMA_HISTOGRAM_LONG_TIMES("Net.NTP.Google.RequestTime2.ErrAborted",
                                 request_loading_time);
      }
    } else {
      if (net_error == net::OK) {
        UMA_HISTOGRAM_LONG_TIMES("Net.NTP.ThirdParty.RequestTime2.Success",
                                 request_loading_time);
      } else if (net_error == net::ERR_ABORTED) {
        UMA_HISTOGRAM_LONG_TIMES("Net.NTP.ThirdParty.RequestTime2.ErrAborted",
                                 request_loading_time);
      }
    }
  } else {
    if (net_error == net::OK) {
      UMA_HISTOGRAM_LONG_TIMES("Net.NTP.Local.RequestTime2.Success",
                               request_loading_time);
    } else if (net_error == net::ERR_ABORTED) {
      UMA_HISTOGRAM_LONG_TIMES("Net.NTP.Local.RequestTime2.ErrAborted",
                               request_loading_time);
    }
  }
}

void NotifyUIThreadOfRequestComplete(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    content::ResourceType resource_type,
    bool was_cached,
    int net_error,
    int64_t total_received_bytes,
    base::TimeDelta request_loading_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    LogMainFrameMetricsOnUIThread(url, net_error, request_loading_time,
                                  web_contents);
  }
  if (!was_cached)
    UpdatePrerenderNetworkBytesCallback(web_contents, total_received_bytes);
  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (metrics_observer) {
    metrics_observer->OnRequestComplete(resource_type, was_cached, net_error);
  }
}

}  // namespace

ChromeResourceDispatcherHostDelegate::ChromeResourceDispatcherHostDelegate()
    : download_request_limiter_(g_browser_process->download_request_limiter()),
      safe_browsing_(g_browser_process->safe_browsing_service())
#if BUILDFLAG(ENABLE_EXTENSIONS)
      , user_script_listener_(new extensions::UserScriptListener())
#endif
      {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(content::ServiceWorkerContext::AddExcludedHeadersForFetchEvent,
                 variations::GetVariationHeaderNames()));
}

ChromeResourceDispatcherHostDelegate::~ChromeResourceDispatcherHostDelegate() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  CHECK(stream_target_info_.empty());
#endif
}

bool ChromeResourceDispatcherHostDelegate::ShouldBeginRequest(
    const std::string& method,
    const GURL& url,
    ResourceType resource_type,
    content::ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Handle a PREFETCH resource type. If prefetch is disabled, squelch the
  // request.  Otherwise, do a normal request to warm the cache.
  if (resource_type == content::RESOURCE_TYPE_PREFETCH) {
    // All PREFETCH requests should be GETs, but be defensive about it.
    if (method != "GET")
      return false;

    // If prefetch is disabled, kill the request.
    std::string prefetch_experiment =
        base::FieldTrialList::FindFullName("Prefetch");
    if (base::StartsWith(prefetch_experiment, "ExperimentDisable",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }

  return true;
}

void ChromeResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    ResourceType resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
  if (safe_browsing_.get())
    safe_browsing_->OnResourceRequest(request);

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

// The lowering of request priority causes issues with scheduling, since
// content::ResourceScheduler uses it to delay and throttle requests. This is
// disabled only on Android, as the prerenders are not likely to compete with
// page loads there.
// See https://crbug.com/652746 for details.
// TODO(lizeb,droger): Fix the issue on all platforms.
#if !defined(OS_ANDROID)
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
#endif  // OS_ANDROID

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(
      resource_context);

#if defined(OS_ANDROID)
  if (resource_type != content::RESOURCE_TYPE_MAIN_FRAME)
    InterceptNavigationDelegate::UpdateUserGestureCarryoverInfo(request);
#endif

#if defined(OS_CHROMEOS)
  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of XHR requests.
  if (resource_type == content::RESOURCE_TYPE_XHR) {
    // Add interstitial page while merge session process (cookie
    // reconstruction from OAuth2 refresh token in ChromeOS login) is still in
    // progress while we are attempting to load a google property.
    if (!merge_session_throttling_utils::AreAllSessionMergedAlready() &&
        request->url().SchemeIsHTTPOrHTTPS()) {
      throttles->push_back(new MergeSessionResourceThrottle(request));
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
    variations::AppendVariationHeaders(
        request->url(), is_off_the_record,
        !is_off_the_record && io_data->GetMetricsEnabledStateOnIOThread(),
        &headers);
    request->SetExtraRequestHeaders(headers);
  }

  if (io_data->policy_header_helper())
    io_data->policy_header_helper()->AddPolicyHeaders(request->url(), request);

  signin::FixMirrorRequestHeaderHelper(request, GURL() /* redirect_url */,
                                       io_data, info->GetChildID(),
                                       info->GetRouteID());

  AppendStandardResourceThrottles(request,
                                  resource_context,
                                  resource_type,
                                  throttles);
#if !defined(DISABLE_NACL)
  AppendComponentUpdaterThrottles(request, *info, resource_context,
                                  resource_type, throttles);
#endif  // !defined(DISABLE_NACL)

  if (io_data->resource_prefetch_predictor_observer()) {
    io_data->resource_prefetch_predictor_observer()->OnRequestStarted(
        request, resource_type, info->GetChildID(), info->GetRenderFrameID());
  }
}

void ChromeResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request,
    ScopedVector<content::ResourceThrottle>* throttles) {
  const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyDownloadInitiatedOnUI,
      info->GetWebContentsGetterForRequest()));

  // If it's from the web, we don't trust it, so we push the throttle on.
  if (is_content_initiated) {
    throttles->push_back(new DownloadResourceThrottle(
        download_request_limiter_, info->GetWebContentsGetterForRequest(),
        request->url(), request->method()));
  }

  // If this isn't a new request, the standard resource throttles have already
  // been added, so no need to add them again.
  if (is_new_request) {
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
    content::ResourceRequestInfo* info) {
  // Get the state, if |url| is in blacklist, whitelist or in none of those.
  ProfileIOData* io_data =
      ProfileIOData::FromResourceContext(info->GetContext());
  const policy::URLBlacklist::URLBlacklistState url_state =
      io_data->GetURLBlacklistState(url);
  if (url_state == policy::URLBlacklist::URLBlacklistState::URL_IN_BLACKLIST) {
    // It's a link with custom scheme and it's blacklisted. We return false here
    // and let it process as a normal URL. Eventually chrome_network_delegate
    // will see it's in the blacklist and the user will be shown the blocked
    // content page.
    return false;
  }
  int child_id = info->GetChildID();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // External protocols are disabled for guests. An exception is made for the
  // "mailto" protocol, so that pages that utilize it work properly in a
  // WebView.
  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(info->GetNavigationUIData());
  if ((extensions::WebViewRendererState::GetInstance()->IsGuest(child_id) ||
      (navigation_data &&
       navigation_data->GetExtensionNavigationUIData()->is_web_view())) &&
      !url.SchemeIs(url::kMailToScheme)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_ANDROID)
  // Main frame external protocols are handled by
  // InterceptNavigationResourceThrottle.
  if (info->IsMainFrame())
    return false;
#endif  // defined(ANDROID)

  const bool is_whitelisted =
      url_state == policy::URLBlacklist::URLBlacklistState::URL_IN_WHITELIST;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&LaunchURL, url, child_id,
                 info->GetWebContentsGetterForRequest(),
                 info->GetPageTransition(), info->HasUserGesture(),
                 is_whitelisted));
  return true;
}

void ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    ScopedVector<content::ResourceThrottle>* throttles) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  // Insert either safe browsing or data reduction proxy throttle at the front
  // of the list, so one of them gets to decide if the resource is safe.
  content::ResourceThrottle* first_throttle = NULL;
#if defined(OS_ANDROID)
  first_throttle = DataReductionProxyResourceThrottle::MaybeCreate(
      request, resource_context, resource_type, safe_browsing_.get());
#endif  // defined(OS_ANDROID)

#if defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)
  if (!first_throttle && io_data->safe_browsing_enabled()->GetValue()) {
    first_throttle = SafeBrowsingResourceThrottle::MaybeCreate(
        request, resource_type, safe_browsing_.get());
  }
#endif  // defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)

  if (first_throttle)
    throttles->push_back(first_throttle);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  std::unique_ptr<content::ResourceThrottle> supervised_user_throttle =
      SupervisedUserResourceThrottle::MaybeCreate(
          request, resource_type, io_data->supervised_user_url_filter());
  if (supervised_user_throttle)
    throttles->push_back(supervised_user_throttle.release());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::ResourceThrottle* wait_for_extensions_init_throttle =
      user_script_listener_->CreateResourceThrottle(request->url(),
                                                    resource_type);
  if (wait_for_extensions_init_throttle)
    throttles->push_back(wait_for_extensions_init_throttle);

  extensions::ExtensionThrottleManager* extension_throttle_manager =
      io_data->GetExtensionThrottleManager();
  if (extension_throttle_manager) {
    std::unique_ptr<content::ResourceThrottle> extension_throttle =
        extension_throttle_manager->MaybeCreateThrottle(request);
    if (extension_throttle)
      throttles->push_back(extension_throttle.release());
  }
#endif

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info->GetVisibilityState() == blink::WebPageVisibilityStatePrerender) {
    throttles->push_back(new prerender::PrerenderResourceThrottle(request));
  }

  std::unique_ptr<PredictorResourceThrottle> predictor_throttle =
      PredictorResourceThrottle::MaybeCreate(request, io_data);
  if (predictor_throttle)
    throttles->push_back(predictor_throttle.release());
}

bool ChromeResourceDispatcherHostDelegate::ShouldForceDownloadResource(
    const GURL& url, const std::string& mime_type) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Special-case user scripts to get downloaded instead of viewed.
  return extensions::UserScript::IsURLUserScript(url, mime_type);
#else
  return false;
#endif
}

bool ChromeResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const base::FilePath& plugin_path,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  ProfileIOData* io_data =
      ProfileIOData::FromResourceContext(info->GetContext());
  bool profile_is_off_the_record = io_data->IsOffTheRecord();
  const scoped_refptr<const extensions::InfoMap> extension_info_map(
      io_data->GetExtensionInfoMap());
  std::vector<std::string> whitelist = MimeTypesHandler::GetMIMETypeWhitelist();
  // Go through the white-listed extensions and try to use them to intercept
  // the URL request.
  for (const std::string& extension_id : whitelist) {
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
    if (!handler)
      continue;

    // If a plugin path is provided then a stream is being intercepted for the
    // mimeHandlerPrivate API. Otherwise a stream is being intercepted for the
    // streamsPrivate API.
    if (!plugin_path.empty()) {
      if (handler->HasPlugin() && plugin_path == handler->GetPluginPath()) {
        StreamTargetInfo target_info;
        *origin = Extension::GetBaseURLFromExtensionId(extension_id);
        target_info.extension_id = extension_id;
        target_info.view_id = base::GenerateGUID();
        *payload = target_info.view_id;
        stream_target_info_[request] = target_info;
        return true;
      }
    } else {
      if (!handler->HasPlugin() && handler->CanHandleMIMEType(mime_type)) {
        StreamTargetInfo target_info;
        *origin = Extension::GetBaseURLFromExtensionId(extension_id);
        target_info.extension_id = extension_id;
        stream_target_info_[request] = target_info;
        return true;
      }
    }
  }
#endif
  return false;
}

void ChromeResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    std::unique_ptr<content::StreamInfo> stream) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  std::map<net::URLRequest*, StreamTargetInfo>::iterator ix =
      stream_target_info_.find(request);
  CHECK(ix != stream_target_info_.end());
  bool embedded = info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SendExecuteMimeTypeHandlerEvent, base::Passed(&stream),
                 request->GetExpectedContentSize(), ix->second.extension_id,
                 ix->second.view_id, embedded, info->GetFrameTreeNodeId(),
                 info->GetChildID(), info->GetRenderFrameID()));
  stream_target_info_.erase(request);
#endif
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  // See if the response contains the X-Chrome-Manage-Accounts header. If so
  // show the profile avatar bubble so that user can complete signin/out action
  // the native UI.
  signin::ProcessMirrorResponseHeaderIfExists(request, io_data,
                                              info->GetChildID(),
                                              info->GetRouteID());

  // Built-in additional protection for the chrome web store origin.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  GURL webstore_url(extension_urls::GetWebstoreLaunchURL());
  if (request->url().SchemeIsHTTPOrHTTPS() &&
      request->url().DomainIs(webstore_url.host().c_str())) {
    net::HttpResponseHeaders* response_headers = request->response_headers();
    if (response_headers &&
        !response_headers->HasHeaderValue("x-frame-options", "deny") &&
        !response_headers->HasHeaderValue("x-frame-options", "sameorigin")) {
      response_headers->RemoveHeader("x-frame-options");
      response_headers->AddHeader("x-frame-options: sameorigin");
    }
  }
#endif

  if (io_data->resource_prefetch_predictor_observer())
    io_data->resource_prefetch_predictor_observer()->OnResponseStarted(request);

  mod_pagespeed::RecordMetrics(info->GetResourceType(), request->url(),
                               request->response_headers());
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // In the Mirror world (for users that are signed in to the browser on
  // Android, the identity is mirrored into the content area), Chrome should
  // append a X-Chrome-Connected header to all Gaia requests from a connected
  // profile so Gaia could return a 204 response and let Chrome handle the
  // action with native UI. The only exception is requests from gaia webview,
  // since the native profile management UI is built on top of it.
  signin::FixMirrorRequestHeaderHelper(request, redirect_url, io_data,
                                       info->GetChildID(), info->GetRouteID());

  if (io_data->resource_prefetch_predictor_observer()) {
    io_data->resource_prefetch_predictor_observer()->OnRequestRedirected(
        redirect_url, request);
  }

  if (io_data->policy_header_helper())
    io_data->policy_header_helper()->AddPolicyHeaders(redirect_url, request);
}

// Notification that a request has completed.
void ChromeResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {
  if (!url_request)
    return;
  // TODO(maksims): remove this and use net_error argument in RequestComplete
  // once ResourceDispatcherHostDelegate is modified.
  int net_error = url_request->status().error();
  const ResourceRequestInfo* info =
      ResourceRequestInfo::ForRequest(url_request);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyUIThreadOfRequestComplete,
                 info->GetWebContentsGetterForRequest(), url_request->url(),
                 info->GetResourceType(), url_request->was_cached(), net_error,
                 url_request->GetTotalReceivedBytes(),
                 base::TimeTicks::Now() - url_request->creation_time()));
}

bool ChromeResourceDispatcherHostDelegate::ShouldEnableLoFiMode(
    const net::URLRequest& url_request,
    content::ResourceContext* resource_context) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  data_reduction_proxy::DataReductionProxyIOData* data_reduction_proxy_io_data =
      io_data->data_reduction_proxy_io_data();

  if (data_reduction_proxy_io_data)
    return data_reduction_proxy_io_data->ShouldEnableLoFiMode(url_request);
  return false;
}

// static
void ChromeResourceDispatcherHostDelegate::
    SetExternalProtocolHandlerDelegateForTesting(
    ExternalProtocolHandler::Delegate* delegate) {
  g_external_protocol_handler_delegate = delegate;
}

content::NavigationData*
ChromeResourceDispatcherHostDelegate::GetNavigationData(
    net::URLRequest* request) const {
  ChromeNavigationData* data =
      ChromeNavigationData::GetDataAndCreateIfNecessary(request);
  if (!request)
    return data;

  data_reduction_proxy::DataReductionProxyData* data_reduction_proxy_data =
      data_reduction_proxy::DataReductionProxyData::GetData(*request);
  // DeepCopy the DataReductionProxyData from the URLRequest to prevent the
  // URLRequest and DataReductionProxyData from both having ownership of the
  // same object. This copy will be shortlived as it will be deep copied again
  // when content makes a clone of NavigationData for the UI thread.
  if (data_reduction_proxy_data)
    data->SetDataReductionProxyData(data_reduction_proxy_data->DeepCopy());
  return data;
}

std::unique_ptr<net::ClientCertStore>
ChromeResourceDispatcherHostDelegate::CreateClientCertStore(
    content::ResourceContext* resource_context) {
  return ProfileIOData::FromResourceContext(resource_context)->
      CreateClientCertStore();
}

// Record RAPPOR for aborted main frame loads. Separate into a fast and
// slow bucket because a shocking number of aborts happen under 100ms.
void ChromeResourceDispatcherHostDelegate::OnAbortedFrameLoad(
    const GURL& url,
    base::TimeDelta request_loading_time) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ChromeResourceDispatcherHostDelegate::OnAbortedFrameLoad,
                    base::Unretained(this), url, request_loading_time));
      return;
  }

  std::string metric_name = (request_loading_time.InMilliseconds() < 100 ?
      "Net.ErrAborted.Fast" : "Net.ErrAborted.Slow");
  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(), metric_name, url);
}
