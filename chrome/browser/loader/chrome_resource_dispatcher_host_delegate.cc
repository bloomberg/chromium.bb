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
#include "base/memory/ptr_util.h"
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
#include "chrome/browser/net/loading_predictor_observer.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_util.h"
#include "components/google/core/browser/google_util.h"
#include "components/nacl/common/features.h"
#include "components/offline_pages/features/features.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_io_data.h"
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
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_response.h"
#include "extensions/features/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/component_updater/pnacl_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
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

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/downloads/resource_throttle.h"
#include "chrome/browser/offline_pages/offliner_user_data.h"
#include "chrome/browser/offline_pages/resource_loading_observer.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/intercept_download_resource_throttle.h"
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
  if (is_whitelisted) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url, web_contents);
  } else {
    ExternalProtocolHandler::LaunchUrlWithDelegate(
        url, web_contents->GetRenderViewHost()->GetProcess()->GetID(),
        web_contents->GetRenderViewHost()->GetRoutingID(), page_transition,
        has_user_gesture, g_external_protocol_handler_delegate);
  }
}

#if BUILDFLAG(ENABLE_NACL)
void AppendComponentUpdaterThrottles(
    net::URLRequest* request,
    const ResourceRequestInfo& info,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  bool is_prerendering =
      info.GetVisibilityState() == blink::kWebPageVisibilityStatePrerender;
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
    throttles->push_back(base::WrapUnique(
        component_updater::GetOnDemandResourceThrottle(cus, crx_id)));
  }
}
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
// Translate content::ResourceType to a type to use for Offliners.
offline_pages::ResourceLoadingObserver::ResourceDataType
ConvertResourceTypeToResourceDataType(content::ResourceType type) {
  switch (type) {
    case content::RESOURCE_TYPE_STYLESHEET:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::TEXT_CSS;
    case content::RESOURCE_TYPE_IMAGE:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::IMAGE;
    case content::RESOURCE_TYPE_XHR:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::XHR;
    default:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::OTHER;
  }
}

void NotifyUIThreadOfRequestStarted(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(petewil) We're not sure why yet, but we do sometimes see that
  // web_contents_getter returning null.  Until we find out why, avoid crashing.
  // crbug.com/742370
  if (web_contents_getter.is_null())
    return;

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  // If we are producing an offline version of the page, track resource loading.
  offline_pages::ResourceLoadingObserver* resource_tracker =
      offline_pages::OfflinerUserData::ResourceLoadingObserverFromWebContents(
          web_contents);
  if (resource_tracker) {
    offline_pages::ResourceLoadingObserver::ResourceDataType data_type =
        ConvertResourceTypeToResourceDataType(resource_type);
    resource_tracker->ObserveResourceLoading(data_type, true /* STARTED */);
  }
}
#endif

void NotifyUIThreadOfRequestComplete(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const content::ResourceRequestInfo::FrameTreeNodeIdGetter&
        frame_tree_node_id_getter,
    const GURL& url,
    const net::HostPortPair& host_port_pair,
    const content::GlobalRequestID& request_id,
    int render_process_id,
    int render_frame_id,
    ResourceType resource_type,
    bool is_download,
    bool was_cached,
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
        data_reduction_proxy_data,
    int net_error,
    int64_t total_received_bytes,
    int64_t raw_body_bytes,
    int64_t original_content_length,
    base::TimeTicks request_creation_time,
    std::unique_ptr<net::LoadTimingInfo> load_timing_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  if (!was_cached) {
    UpdatePrerenderNetworkBytesCallback(web_contents, total_received_bytes);
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // If we are producing an offline version of the page, track resource loading.
  offline_pages::ResourceLoadingObserver* resource_tracker =
      offline_pages::OfflinerUserData::ResourceLoadingObserverFromWebContents(
          web_contents);
  if (resource_tracker) {
    offline_pages::ResourceLoadingObserver::ResourceDataType data_type =
        ConvertResourceTypeToResourceDataType(resource_type);
    resource_tracker->ObserveResourceLoading(data_type, false /* COMPLETED */);
    if (!was_cached)
      resource_tracker->OnNetworkBytesChanged(total_received_bytes);
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  if (!is_download) {
    page_load_metrics::MetricsWebContentsObserver* metrics_observer =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            web_contents);
    if (metrics_observer) {
      // Will be null for main or sub frame resources, when browser-side
      // navigation is enabled.
      content::RenderFrameHost* render_frame_host_or_null =
          content::RenderFrameHost::FromID(render_process_id, render_frame_id);
      metrics_observer->OnRequestComplete(
          url, host_port_pair, frame_tree_node_id_getter.Run(), request_id,
          render_frame_host_or_null, resource_type, was_cached,
          std::move(data_reduction_proxy_data), raw_body_bytes,
          original_content_length, request_creation_time, net_error,
          std::move(load_timing_info));
    }
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
      base::BindOnce(
          content::ServiceWorkerContext::AddExcludedHeadersForFetchEvent,
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
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  if (safe_browsing_.get())
    safe_browsing_->OnResourceRequest(request);

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // TODO(petewil): Unify the safe browsing request and the metrics observer
  // request if possible so we only have to cross to the main thread once.
  // http://crbug.com/712312.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&NotifyUIThreadOfRequestStarted,
                                         info->GetWebContentsGetterForRequest(),
                                         info->GetResourceType()));
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

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
      throttles->push_back(
          base::MakeUnique<MergeSessionResourceThrottle>(request));
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
    bool is_signed_in =
        !is_off_the_record &&
        !io_data->google_services_account_id()->GetValue().empty();
    variations::AppendVariationHeaders(
        request->url(), is_off_the_record, is_signed_in,
        !is_off_the_record && io_data->GetMetricsEnabledStateOnIOThread(),
        &headers);
    request->SetExtraRequestHeaders(headers);
  }

  if (io_data->policy_header_helper())
    io_data->policy_header_helper()->AddPolicyHeaders(request->url(), request);

  signin::FixAccountConsistencyRequestHeader(request, GURL() /* redirect_url */,
                                             io_data);

  AppendStandardResourceThrottles(request,
                                  resource_context,
                                  resource_type,
                                  throttles);
#if BUILDFLAG(ENABLE_NACL)
  AppendComponentUpdaterThrottles(request, *info, resource_context,
                                  resource_type, throttles);
#endif  // BUILDFLAG(ENABLE_NACL)

  if (io_data->loading_predictor_observer()) {
    io_data->loading_predictor_observer()->OnRequestStarted(
        request, resource_type, info->GetWebContentsGetterForRequest());
  }
}

void ChromeResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&NotifyDownloadInitiatedOnUI,
                     info->GetWebContentsGetterForRequest()));

  // If it's from the web, we don't trust it, so we push the throttle on.
  if (is_content_initiated) {
    throttles->push_back(base::MakeUnique<DownloadResourceThrottle>(
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
#if defined(OS_ANDROID)
    // On Android, forward text/html downloads to OfflinePages backend.
    throttles->push_back(
        base::MakeUnique<offline_pages::downloads::ResourceThrottle>(request));
#endif
  }

#if defined(OS_ANDROID)
  // Add the InterceptDownloadResourceThrottle after calling
  // AppendStandardResourceThrottles so the download will not bypass
  // safebrowsing checks.
  if (is_content_initiated) {
    throttles->push_back(base::MakeUnique<InterceptDownloadResourceThrottle>(
        request, info->GetWebContentsGetterForRequest()));
  }
#endif
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // External protocols are disabled for guests. An exception is made for the
  // "mailto" protocol, so that pages that utilize it work properly in a
  // WebView.
  int child_id = info->GetChildID();
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
      base::BindOnce(&LaunchURL, url, info->GetWebContentsGetterForRequest(),
                     info->GetPageTransition(), info->HasUserGesture(),
                     is_whitelisted));
  return true;
}

void ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
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
    first_throttle = MaybeCreateSafeBrowsingResourceThrottle(
        request, resource_type, safe_browsing_.get());
  }
#endif  // defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)

  if (first_throttle)
    throttles->push_back(base::WrapUnique(first_throttle));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::ResourceThrottle* wait_for_extensions_init_throttle =
      user_script_listener_->CreateResourceThrottle(request->url(),
                                                    resource_type);
  if (wait_for_extensions_init_throttle)
    throttles->push_back(base::WrapUnique(wait_for_extensions_init_throttle));

  extensions::ExtensionThrottleManager* extension_throttle_manager =
      io_data->GetExtensionThrottleManager();
  if (extension_throttle_manager) {
    std::unique_ptr<content::ResourceThrottle> extension_throttle =
        extension_throttle_manager->MaybeCreateThrottle(request);
    if (extension_throttle)
      throttles->push_back(std::move(extension_throttle));
  }
#endif

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info->GetVisibilityState() == blink::kWebPageVisibilityStatePrerender) {
    throttles->push_back(
        base::MakeUnique<prerender::PrerenderResourceThrottle>(request));
  }

  std::unique_ptr<PredictorResourceThrottle> predictor_throttle =
      PredictorResourceThrottle::MaybeCreate(request, io_data);
  if (predictor_throttle)
    throttles->push_back(std::move(predictor_throttle));
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
      base::BindOnce(&SendExecuteMimeTypeHandlerEvent, base::Passed(&stream),
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

  signin::ProcessAccountConsistencyResponseHeaders(request, GURL(),
                                                   io_data->IsOffTheRecord());

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

  if (io_data->loading_predictor_observer())
    io_data->loading_predictor_observer()->OnResponseStarted(
        request, info->GetWebContentsGetterForRequest());

  mod_pagespeed::RecordMetrics(info->GetResourceType(), request->url(),
                               request->response_headers());
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceResponse* response) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  // Chrome tries to ensure that the identity is consistent between Chrome and
  // the content area.
  //
  // For example, on Android, for users that are signed in to Chrome, the
  // identity is mirrored into the content area. To do so, Chrome appends a
  // X-Chrome-Connected header to all Gaia requests from a connected profile so
  // Gaia could return a 204 response and let Chrome handle the action with
  // native UI.
  signin::FixAccountConsistencyRequestHeader(request, redirect_url, io_data);
  signin::ProcessAccountConsistencyResponseHeaders(request, redirect_url,
                                                   io_data->IsOffTheRecord());

  if (io_data->loading_predictor_observer()) {
    const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
    io_data->loading_predictor_observer()->OnRequestRedirected(
        request, redirect_url, info->GetWebContentsGetterForRequest());
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

  data_reduction_proxy::DataReductionProxyData* data =
      data_reduction_proxy::DataReductionProxyData::GetData(*url_request);
  std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
      data_reduction_proxy_data;
  if (data)
    data_reduction_proxy_data = data->DeepCopy();
  int64_t original_content_length =
      data && data->used_data_reduction_proxy()
          ? data_reduction_proxy::util::CalculateEffectiveOCL(*url_request)
          : url_request->GetRawBodyBytes();

  net::HostPortPair request_host_port;
  // We want to get the IP address of the response if it was returned, and the
  // last endpoint that was checked if it failed.
  if (url_request->response_headers()) {
    request_host_port = url_request->GetSocketAddress();
  }
  if (request_host_port.IsEmpty()) {
    net::IPEndPoint request_ip_endpoint;
    bool was_successful = url_request->GetRemoteEndpoint(&request_ip_endpoint);
    if (was_successful) {
      request_host_port =
          net::HostPortPair::FromIPEndPoint(request_ip_endpoint);
    }
  }

  auto load_timing_info = base::MakeUnique<net::LoadTimingInfo>();
  url_request->GetLoadTimingInfo(load_timing_info.get());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &NotifyUIThreadOfRequestComplete,
          info->GetWebContentsGetterForRequest(),
          info->GetFrameTreeNodeIdGetterForRequest(), url_request->url(),
          request_host_port, info->GetGlobalRequestID(), info->GetChildID(),
          info->GetRenderFrameID(), info->GetResourceType(), info->IsDownload(),
          url_request->was_cached(), std::move(data_reduction_proxy_data),
          net_error, url_request->GetTotalReceivedBytes(),
          url_request->GetRawBodyBytes(), original_content_length,
          url_request->creation_time(),
          std::move(load_timing_info)));
}

content::PreviewsState ChromeResourceDispatcherHostDelegate::GetPreviewsState(
    const net::URLRequest& url_request,
    content::ResourceContext* resource_context,
    content::PreviewsState previews_to_allow) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  data_reduction_proxy::DataReductionProxyIOData* data_reduction_proxy_io_data =
      io_data->data_reduction_proxy_io_data();

  content::PreviewsState previews_state = content::PREVIEWS_UNSPECIFIED;

  previews::PreviewsIOData* previews_io_data = io_data->previews_io_data();
  if (data_reduction_proxy_io_data && previews_io_data) {
    if (data_reduction_proxy_io_data->ShouldEnableLoFi(url_request,
                                                       previews_io_data)) {
      previews_state |= content::SERVER_LOFI_ON;
    }
    if (data_reduction_proxy_io_data->ShouldEnableLitePages(url_request,
                                                            previews_io_data)) {
      previews_state |= content::SERVER_LITE_PAGE_ON;
    }

    // Check that data saver is enabled and the user is eligible for Lo-Fi
    // previews. If the user is not transitioned fully to the blacklist, respect
    // the old prefs rules.
    if (data_reduction_proxy_io_data->IsEnabled() &&
        previews::params::IsClientLoFiEnabled() &&
        previews_io_data->ShouldAllowPreviewAtECT(
            url_request, previews::PreviewsType::LOFI,
            previews::params::EffectiveConnectionTypeThresholdForClientLoFi(),
            previews::params::GetBlackListedHostsForClientLoFiFieldTrial())) {
      previews_state |= content::CLIENT_LOFI_ON;
    }
  }

  if (previews_state == content::PREVIEWS_UNSPECIFIED)
    return content::PREVIEWS_OFF;

  // If the allowed previews are limited, ensure we honor those limits.
  if (previews_to_allow != content::PREVIEWS_UNSPECIFIED &&
      previews_state != content::PREVIEWS_OFF &&
      previews_state != content::PREVIEWS_NO_TRANSFORM) {
    previews_state = previews_state & previews_to_allow;
    // If no valid previews are left, set the state explictly to PREVIEWS_OFF.
    if (previews_state == 0)
      previews_state = content::PREVIEWS_OFF;
  }
  return previews_state;
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

  // Update the previews state from the navigation data.
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (info) {
    data->set_previews_state(info->GetPreviewsState());
  }

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
