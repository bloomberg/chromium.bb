// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include <stdlib.h>

#include <vector>

#include "base/base_paths.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/net/chrome_extensions_network_delegate.h"
#include "chrome/browser/net/client_hints.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/safe_search_util.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "components/domain_reliability/monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_ANDROID)
#include "chrome/browser/io_thread.h"
#include "components/precache/content/precache_manager.h"
#include "components/precache/content/precache_manager_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/url_blacklist_manager.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;
using content::ResourceType;

// By default we don't allow access to all file:// urls on ChromeOS and
// Android.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
bool ChromeNetworkDelegate::g_allow_file_access_ = false;
#else
bool ChromeNetworkDelegate::g_allow_file_access_ = true;
#endif

#if defined(ENABLE_EXTENSIONS)
// This remains false unless the --disable-extensions-http-throttling
// flag is passed to the browser.
bool ChromeNetworkDelegate::g_never_throttle_requests_ = false;
#endif

namespace {

const char kDNTHeader[] = "DNT";

// Gets called when the extensions finish work on the URL. If the extensions
// did not do a redirect (so |new_url| is empty) then we enforce the
// SafeSearch parameters. Otherwise we will get called again after the
// redirect and we enforce SafeSearch then.
void ForceGoogleSafeSearchCallbackWrapper(
    const net::CompletionCallback& callback,
    net::URLRequest* request,
    GURL* new_url,
    int rv) {
  if (rv == net::OK && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request, new_url);
  callback.Run(rv);
}

void UpdateContentLengthPrefs(
    int received_content_length,
    int original_content_length,
    data_reduction_proxy::DataReductionProxyRequestType request_type,
    Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(received_content_length, 0);
  DCHECK_GE(original_content_length, 0);

  // Can be NULL in a unit test.
  if (!g_browser_process)
    return;

  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return;

  // Ignore off-the-record data.
  if (!g_browser_process->profile_manager()->IsValidProfile(profile) ||
      profile->IsOffTheRecord()) {
    return;
  }
#if defined(OS_ANDROID)
  // If Android ever goes multi profile, the profile should be passed so that
  // the browser preference will be taken.
  bool with_data_reduction_proxy_enabled =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          data_reduction_proxy::prefs::kDataReductionProxyEnabled);
#else
  bool with_data_reduction_proxy_enabled = false;
#endif

  data_reduction_proxy::UpdateContentLengthPrefs(received_content_length,
                                         original_content_length,
                                         with_data_reduction_proxy_enabled,
                                         request_type, prefs);
}

void StoreAccumulatedContentLength(
    int received_content_length,
    int original_content_length,
    data_reduction_proxy::DataReductionProxyRequestType request_type,
    Profile* profile) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateContentLengthPrefs,
                 received_content_length, original_content_length,
                 request_type, profile));
}

void RecordContentLengthHistograms(
    int64 received_content_length,
    int64 original_content_length,
    const base::TimeDelta& freshness_lifetime) {
  // Add the current resource to these histograms only when a valid
  // X-Original-Content-Length header is present.
  if (original_content_length >= 0) {
    UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthWithValidOCL",
                         received_content_length);
    UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLengthWithValidOCL",
                         original_content_length);
    UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifferenceWithValidOCL",
                         original_content_length - received_content_length);
  } else {
    // Presume the original content length is the same as the received content
    // length if the X-Original-Content-Header is not present.
    original_content_length = received_content_length;
  }
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLength", received_content_length);
  UMA_HISTOGRAM_COUNTS("Net.HttpOriginalContentLength",
                       original_content_length);
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthDifference",
                       original_content_length - received_content_length);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.HttpContentFreshnessLifetime",
                              freshness_lifetime.InSeconds(),
                              base::TimeDelta::FromHours(1).InSeconds(),
                              base::TimeDelta::FromDays(30).InSeconds(),
                              100);
  if (freshness_lifetime.InSeconds() <= 0)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable",
                       received_content_length);
  if (freshness_lifetime.InHours() < 4)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable4Hours",
                       received_content_length);

  if (freshness_lifetime.InHours() < 24)
    return;
  UMA_HISTOGRAM_COUNTS("Net.HttpContentLengthCacheable24Hours",
                       received_content_length);
}

#if defined(OS_ANDROID)
void RecordPrecacheStatsOnUIThread(const GURL& url,
                                   const base::Time& fetch_time, int64 size,
                                   bool was_cached, void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile)) {
    return;
  }

  precache::PrecacheManager* precache_manager =
      precache::PrecacheManagerFactory::GetForBrowserContext(profile);
  if (!precache_manager) {
    // This could be NULL if the profile is off the record.
    return;
  }

  precache_manager->RecordStatsForFetch(url, fetch_time, size, was_cached);
}

void RecordIOThreadToRequestStartOnUIThread(
    const base::TimeTicks& request_start) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeDelta request_lag = request_start -
      g_browser_process->io_thread()->creation_time();
  UMA_HISTOGRAM_TIMES("Net.IOThreadCreationToHTTPRequestStart", request_lag);
}
#endif  // defined(OS_ANDROID)

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate(
    extensions::EventRouterForwarder* event_router,
    BooleanPrefMember* enable_referrers)
    : profile_(NULL),
      enable_referrers_(enable_referrers),
      enable_do_not_track_(NULL),
      force_google_safe_search_(NULL),
#if defined(ENABLE_CONFIGURATION_POLICY)
      url_blacklist_manager_(NULL),
#endif
      domain_reliability_monitor_(NULL),
      received_content_length_(0),
      original_content_length_(0),
      first_request_(true),
      prerender_tracker_(NULL),
      data_reduction_proxy_params_(NULL),
      data_reduction_proxy_usage_stats_(NULL),
      data_reduction_proxy_auth_request_handler_(NULL) {
  DCHECK(enable_referrers);
  extensions_delegate_.reset(
      ChromeExtensionsNetworkDelegate::Create(event_router));
}

ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

void ChromeNetworkDelegate::set_extension_info_map(
    extensions::InfoMap* extension_info_map) {
  extensions_delegate_->set_extension_info_map(extension_info_map);
}

void ChromeNetworkDelegate::set_profile(void* profile) {
  profile_ = profile;
  extensions_delegate_->set_profile(profile);
}

void ChromeNetworkDelegate::set_cookie_settings(
    CookieSettings* cookie_settings) {
  cookie_settings_ = cookie_settings;
}

void ChromeNetworkDelegate::set_predictor(
    chrome_browser_net::Predictor* predictor) {
  connect_interceptor_.reset(
      new chrome_browser_net::ConnectInterceptor(predictor));
}

void ChromeNetworkDelegate::SetEnableClientHints() {
  client_hints_.reset(new ClientHints());
  client_hints_->Init();
}

// static
#if defined(ENABLE_EXTENSIONS)
void ChromeNetworkDelegate::NeverThrottleRequests() {
  g_never_throttle_requests_ = true;
}
#endif

// static
void ChromeNetworkDelegate::InitializePrefsOnUIThread(
    BooleanPrefMember* enable_referrers,
    BooleanPrefMember* enable_do_not_track,
    BooleanPrefMember* force_google_safe_search,
    PrefService* pref_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_referrers->Init(prefs::kEnableReferrers, pref_service);
  enable_referrers->MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  if (enable_do_not_track) {
    enable_do_not_track->Init(prefs::kEnableDoNotTrack, pref_service);
    enable_do_not_track->MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }
  if (force_google_safe_search) {
    force_google_safe_search->Init(prefs::kForceSafeSearch, pref_service);
    force_google_safe_search->MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }
}

// static
void ChromeNetworkDelegate::AllowAccessToAllFiles() {
  g_allow_file_access_ = true;
}

// static
base::Value* ChromeNetworkDelegate::HistoricNetworkStatsInfoToValue() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* prefs = g_browser_process->local_state();
  int64 total_received = prefs->GetInt64(
      data_reduction_proxy::prefs::kHttpReceivedContentLength);
  int64 total_original = prefs->GetInt64(
      data_reduction_proxy::prefs::kHttpOriginalContentLength);

  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow.  base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return dict;
}

base::Value* ChromeNetworkDelegate::SessionNetworkStatsInfoToValue() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow.  base::Value only supports 32-bit integers.
  dict->SetString("session_received_content_length",
                  base::Int64ToString(received_content_length_));
  dict->SetString("session_original_content_length",
                  base::Int64ToString(original_content_length_));
  return dict;
}

int ChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
#if defined(OS_ANDROID)
  // This UMA tracks the time to the first user-initiated request start, so
  // only non-null profiles are considered.
  if (first_request_ && profile_) {
    bool record_timing = true;
    if (data_reduction_proxy_params_) {
      record_timing =
          (request->url() != data_reduction_proxy_params_->probe_url()) &&
          (request->url() != data_reduction_proxy_params_->warmup_url());
    }
    if (record_timing) {
      first_request_ = false;
      net::LoadTimingInfo timing_info;
      request->GetLoadTimingInfo(&timing_info);
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&RecordIOThreadToRequestStartOnUIThread,
                     timing_info.request_start));
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(ENABLE_CONFIGURATION_POLICY)
  // TODO(joaodasilva): This prevents extensions from seeing URLs that are
  // blocked. However, an extension might redirect the request to another URL,
  // which is not blocked.
  int error = net::ERR_BLOCKED_BY_ADMINISTRATOR;
  if (url_blacklist_manager_ &&
      url_blacklist_manager_->IsRequestBlocked(*request, &error)) {
    // URL access blocked by policy.
    request->net_log().AddEvent(
        net::NetLog::TYPE_CHROME_POLICY_ABORTED_REQUEST,
        net::NetLog::StringCallback("url",
                                    &request->url().possibly_invalid_spec()));
    return error;
  }
#endif

  extensions_delegate_->ForwardStartRequestStatus(request);

  if (!enable_referrers_->GetValue())
    request->SetReferrer(std::string());
  if (enable_do_not_track_ && enable_do_not_track_->GetValue())
    request->SetExtraRequestHeaderByName(kDNTHeader, "1", true /* override */);

  if (client_hints_) {
    request->SetExtraRequestHeaderByName(
        ClientHints::kDevicePixelRatioHeader,
        client_hints_->GetDevicePixelRatioHeader(), true);
  }

  bool force_safe_search = force_google_safe_search_ &&
                           force_google_safe_search_->GetValue();

  net::CompletionCallback wrapped_callback = callback;
  if (force_safe_search) {
    wrapped_callback = base::Bind(&ForceGoogleSafeSearchCallbackWrapper,
                                  callback,
                                  base::Unretained(request),
                                  base::Unretained(new_url));
  }

  int rv = extensions_delegate_->OnBeforeURLRequest(
      request, wrapped_callback, new_url);

  if (force_safe_search && rv == net::OK && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request, new_url);

  if (connect_interceptor_)
    connect_interceptor_->WitnessURLRequest(request);

  return rv;
}

void ChromeNetworkDelegate::OnResolveProxy(
    const GURL& url, int load_flags, net::ProxyInfo* result) {
  if (!on_resolve_proxy_handler_.is_null()) {
    on_resolve_proxy_handler_.Run(url, load_flags,
                                  data_reduction_proxy_params_, result);
  }
}

int ChromeNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  bool force_safe_search = force_google_safe_search_ &&
                           force_google_safe_search_->GetValue();
  if (force_safe_search)
    safe_search_util::ForceYouTubeSafetyMode(request, headers);

  TRACE_EVENT_ASYNC_STEP_PAST0("net", "URLRequest", request, "SendRequest");
  return extensions_delegate_->OnBeforeSendHeaders(request, callback, headers);
}

void ChromeNetworkDelegate::OnBeforeSendProxyHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  if (data_reduction_proxy_auth_request_handler_) {
    data_reduction_proxy_auth_request_handler_->MaybeAddRequestHeader(
        request, proxy_info.proxy_server(), headers);
  }
}

void ChromeNetworkDelegate::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  extensions_delegate_->OnSendHeaders(request, headers);
}

int ChromeNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  if (data_reduction_proxy::MaybeBypassProxyAndPrepareToRetry(
      data_reduction_proxy_params_,
      request,
      original_response_headers,
      override_response_headers)) {
    return net::OK;
  }

  return extensions_delegate_->OnHeadersReceived(
      request,
      callback,
      original_response_headers,
      override_response_headers,
      allowed_unsafe_redirect_url);
}

void ChromeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                             const GURL& new_location) {
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnBeforeRedirect(request);
  extensions_delegate_->OnBeforeRedirect(request, new_location);
}


void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  TRACE_EVENT_ASYNC_STEP_PAST0("net", "URLRequest", request, "ResponseStarted");
  extensions_delegate_->OnResponseStarted(request);
}

void ChromeNetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                           int bytes_read) {
  TRACE_EVENT_ASYNC_STEP_PAST1("net", "URLRequest", &request, "DidRead",
                               "bytes_read", bytes_read);
  performance_monitor::PerformanceMonitor::GetInstance()->BytesReadOnIOThread(
      request, bytes_read);

#if defined(ENABLE_TASK_MANAGER)
  // This is not completely accurate, but as a first approximation ignore
  // requests that are served from the cache. See bug 330931 for more info.
  if (!request.was_cached())
    TaskManager::GetInstance()->model()->NotifyBytesRead(request, bytes_read);
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                        bool started) {
  if (data_reduction_proxy_usage_stats_)
    data_reduction_proxy_usage_stats_->OnUrlRequestCompleted(request, started);

  TRACE_EVENT_ASYNC_END0("net", "URLRequest", request);
  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    // For better accuracy, we use the actual bytes read instead of the length
    // specified with the Content-Length header, which may be inaccurate,
    // or missing, as is the case with chunked encoding.
    int64 received_content_length = request->received_response_content_length();

#if defined(OS_ANDROID)
    if (precache::PrecacheManager::IsPrecachingEnabled()) {
      // Record precache metrics when a fetch is completed successfully, if
      // precaching is enabled.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&RecordPrecacheStatsOnUIThread, request->url(),
                     base::Time::Now(), received_content_length,
                     request->was_cached(), profile_));
    }
#endif  // defined(OS_ANDROID)

    // Only record for http or https urls.
    bool is_http = request->url().SchemeIs("http");
    bool is_https = request->url().SchemeIs("https");

    if (!request->was_cached() &&         // Don't record cached content
        received_content_length &&        // Zero-byte responses aren't useful.
        (is_http || is_https)) {          // Only record for HTTP or HTTPS urls.
      int64 original_content_length =
          request->response_info().headers->GetInt64HeaderValue(
              "x-original-content-length");
      data_reduction_proxy::DataReductionProxyRequestType request_type =
          data_reduction_proxy::GetDataReductionProxyRequestType(request);

      base::TimeDelta freshness_lifetime =
          request->response_info().headers->GetFreshnessLifetime(
              request->response_info().response_time);
      int64 adjusted_original_content_length =
          data_reduction_proxy::GetAdjustedOriginalContentLength(
              request_type, original_content_length,
              received_content_length);
      AccumulateContentLength(received_content_length,
                              adjusted_original_content_length,
                              request_type);
      RecordContentLengthHistograms(received_content_length,
                                    original_content_length,
                                    freshness_lifetime);
      DVLOG(2) << __FUNCTION__
          << " received content length: " << received_content_length
          << " original content length: " << original_content_length
          << " url: " << request->url();
    }

    extensions_delegate_->OnCompleted(request, started);
  } else if (request->status().status() == net::URLRequestStatus::FAILED ||
             request->status().status() == net::URLRequestStatus::CANCELED) {
    extensions_delegate_->OnCompleted(request, started);
  } else {
    NOTREACHED();
  }
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnCompleted(request, started);
  extensions_delegate_->ForwardProxyErrors(request);
  extensions_delegate_->ForwardDoneRequestStatus(request);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  extensions_delegate_->OnURLRequestDestroyed(request);
}

void ChromeNetworkDelegate::OnPACScriptError(int line_number,
                                             const base::string16& error) {
  extensions_delegate_->OnPACScriptError(line_number, error);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return extensions_delegate_->OnAuthRequired(
      request, auth_info, callback, credentials);
}

bool ChromeNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list) {
  // NULL during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return true;

  bool allow = cookie_settings_->IsReadingCookieAllowed(
      request.url(), request.first_party_for_cookies());

  int render_process_id = -1;
  int render_frame_id = -1;

  // |is_for_blocking_resource| indicates whether the cookies read were for a
  // blocking resource (eg script, css). It is only temporarily added for
  // diagnostic purposes, per bug 353678. Will be removed again once data
  // collection is finished.
  bool is_for_blocking_resource = false;
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(&request);
  if (info && ((!info->IsAsync()) ||
               info->GetResourceType() == ResourceType::STYLESHEET ||
               info->GetResourceType() == ResourceType::SCRIPT)) {
    is_for_blocking_resource = true;
  }

  if (content::ResourceRequestInfo::GetRenderFrameForRequest(
          &request, &render_process_id, &render_frame_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookiesRead,
                   render_process_id, render_frame_id,
                   request.url(), request.first_party_for_cookies(),
                   cookie_list, !allow, is_for_blocking_resource));
  }

  return allow;
}

bool ChromeNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                           const std::string& cookie_line,
                                           net::CookieOptions* options) {
  // NULL during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return true;

  bool allow = cookie_settings_->IsSettingCookieAllowed(
      request.url(), request.first_party_for_cookies());

  int render_process_id = -1;
  int render_frame_id = -1;
  if (content::ResourceRequestInfo::GetRenderFrameForRequest(
          &request, &render_process_id, &render_frame_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookieChanged,
                   render_process_id, render_frame_id,
                   request.url(), request.first_party_for_cookies(),
                   cookie_line, *options, !allow));
  }

  if (prerender_tracker_) {
    prerender_tracker_->OnCookieChangedForURL(
        render_process_id,
        request.context()->cookie_store()->GetCookieMonster(),
        request.url());
  }

  return allow;
}

bool ChromeNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                            const base::FilePath& path) const {
  if (g_allow_file_access_)
    return true;

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  return true;
#else
#if defined(OS_CHROMEOS)
  // If we're running Chrome for ChromeOS on Linux, we want to allow file
  // access.
  if (!base::SysInfo::IsRunningOnChromeOS() ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    return true;
  }

  // Use a whitelist to only allow access to files residing in the list of
  // directories below.
  static const char* const kLocalAccessWhiteList[] = {
      "/home/chronos/user/Downloads",
      "/home/chronos/user/log",
      "/home/chronos/user/WebRTC Logs",
      "/media",
      "/opt/oem",
      "/usr/share/chromeos-assets",
      "/tmp",
      "/var/log",
  };

  // The actual location of "/home/chronos/user/Xyz" is the Xyz directory under
  // the profile path ("/home/chronos/user' is a hard link to current primary
  // logged in profile.) For the support of multi-profile sessions, we are
  // switching to use explicit "$PROFILE_PATH/Xyz" path and here whitelist such
  // access.
  if (!profile_path_.empty()) {
    const base::FilePath downloads = profile_path_.AppendASCII("Downloads");
    if (downloads == path.StripTrailingSeparators() || downloads.IsParent(path))
      return true;
    const base::FilePath webrtc_logs = profile_path_.AppendASCII("WebRTC Logs");
    if (webrtc_logs == path.StripTrailingSeparators() ||
        webrtc_logs.IsParent(path)) {
      return true;
    }
  }
#elif defined(OS_ANDROID)
  // Access to files in external storage is allowed.
  base::FilePath external_storage_path;
  PathService::Get(base::DIR_ANDROID_EXTERNAL_STORAGE, &external_storage_path);
  if (external_storage_path.IsParent(path))
    return true;

  // Whitelist of other allowed directories.
  static const char* const kLocalAccessWhiteList[] = {
      "/sdcard",
      "/mnt/sdcard",
  };
#endif

  for (size_t i = 0; i < arraysize(kLocalAccessWhiteList); ++i) {
    const base::FilePath white_listed_path(kLocalAccessWhiteList[i]);
    // base::FilePath::operator== should probably handle trailing separators.
    if (white_listed_path == path.StripTrailingSeparators() ||
        white_listed_path.IsParent(path)) {
      return true;
    }
  }

  DVLOG(1) << "File access denied - " << path.value().c_str();
  return false;
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

bool ChromeNetworkDelegate::OnCanThrottleRequest(
    const net::URLRequest& request) const {
#if defined(ENABLE_EXTENSIONS)
  if (g_never_throttle_requests_)
    return false;
  return request.first_party_for_cookies().scheme() ==
      extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ChromeNetworkDelegate::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  // NULL during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return false;

  bool reading_cookie_allowed = cookie_settings_->IsReadingCookieAllowed(
      url, first_party_for_cookies);
  bool setting_cookie_allowed = cookie_settings_->IsSettingCookieAllowed(
      url, first_party_for_cookies);
  bool privacy_mode = !(reading_cookie_allowed && setting_cookie_allowed);
  return privacy_mode;
}

int ChromeNetworkDelegate::OnBeforeSocketStreamConnect(
    net::SocketStream* socket,
    const net::CompletionCallback& callback) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (url_blacklist_manager_ &&
      url_blacklist_manager_->IsURLBlocked(socket->url())) {
    // URL access blocked by policy.
    socket->net_log()->AddEvent(
        net::NetLog::TYPE_CHROME_POLICY_ABORTED_REQUEST,
        net::NetLog::StringCallback("url",
                                    &socket->url().possibly_invalid_spec()));
    return net::ERR_BLOCKED_BY_ADMINISTRATOR;
  }
#endif
  return net::OK;
}

void ChromeNetworkDelegate::AccumulateContentLength(
    int64 received_content_length,
    int64 original_content_length,
    data_reduction_proxy::DataReductionProxyRequestType request_type) {
  DCHECK_GE(received_content_length, 0);
  DCHECK_GE(original_content_length, 0);
  StoreAccumulatedContentLength(received_content_length,
                                original_content_length,
                                request_type,
                                reinterpret_cast<Profile*>(profile_));
  received_content_length_ += received_content_length;
  original_content_length_ += original_content_length;
}
