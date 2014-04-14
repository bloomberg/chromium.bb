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
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/client_hints.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_metrics.h"
#include "components/domain_reliability/monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/url_blacklist_manager.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/io_thread.h"
#include "components/precache/content/precache_manager.h"
#include "components/precache/content/precache_manager_factory.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;

// By default we don't allow access to all file:// urls on ChromeOS and
// Android.
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
bool ChromeNetworkDelegate::g_allow_file_access_ = false;
#else
bool ChromeNetworkDelegate::g_allow_file_access_ = true;
#endif

// This remains false unless the --disable-extensions-http-throttling
// flag is passed to the browser.
bool ChromeNetworkDelegate::g_never_throttle_requests_ = false;

namespace {

const char kDNTHeader[] = "DNT";

// If the |request| failed due to problems with a proxy, forward the error to
// the proxy extension API.
void ForwardProxyErrors(net::URLRequest* request,
                        extensions::EventRouterForwarder* event_router,
                        void* profile) {
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    switch (request->status().error()) {
      case net::ERR_PROXY_AUTH_UNSUPPORTED:
      case net::ERR_PROXY_CONNECTION_FAILED:
      case net::ERR_TUNNEL_CONNECTION_FAILED:
        extensions::ProxyEventRouter::GetInstance()->OnProxyError(
            event_router, profile, request->status().error());
    }
  }
}

// Returns whether a URL parameter, |first_parameter| (e.g. foo=bar), has the
// same key as the the |second_parameter| (e.g. foo=baz). Both parameters
// must be in key=value form.
bool HasSameParameterKey(const std::string& first_parameter,
                         const std::string& second_parameter) {
  DCHECK(second_parameter.find("=") != std::string::npos);
  // Prefix for "foo=bar" is "foo=".
  std::string parameter_prefix = second_parameter.substr(
      0, second_parameter.find("=") + 1);
  return StartsWithASCII(first_parameter, parameter_prefix, false);
}

// Examines the query string containing parameters and adds the necessary ones
// so that SafeSearch is active. |query| is the string to examine and the
// return value is the |query| string modified such that SafeSearch is active.
std::string AddSafeSearchParameters(const std::string& query) {
  std::vector<std::string> new_parameters;
  std::string safe_parameter = chrome::kSafeSearchSafeParameter;
  std::string ssui_parameter = chrome::kSafeSearchSsuiParameter;

  std::vector<std::string> parameters;
  base::SplitString(query, '&', &parameters);

  std::vector<std::string>::iterator it;
  for (it = parameters.begin(); it < parameters.end(); ++it) {
    if (!HasSameParameterKey(*it, safe_parameter) &&
        !HasSameParameterKey(*it, ssui_parameter)) {
      new_parameters.push_back(*it);
    }
  }

  new_parameters.push_back(safe_parameter);
  new_parameters.push_back(ssui_parameter);
  return JoinString(new_parameters, '&');
}

// If |request| is a request to Google Web Search the function
// enforces that the SafeSearch query parameters are set to active.
// Sets the query part of |new_url| with the new value of the parameters.
void ForceGoogleSafeSearch(net::URLRequest* request,
                           GURL* new_url) {
  if (!google_util::IsGoogleSearchUrl(request->url()) &&
      !google_util::IsGoogleHomePageUrl(request->url()))
    return;

  std::string query = request->url().query();
  std::string new_query = AddSafeSearchParameters(query);
  if (query == new_query)
    return;

  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query);
  *new_url = request->url().ReplaceComponents(replacements);
}

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
    ForceGoogleSafeSearch(request, new_url);
  callback.Run(rv);
}

enum RequestStatus { REQUEST_STARTED, REQUEST_DONE };

// Notifies the extensions::ProcessManager that a request has started or stopped
// for a particular RenderFrame.
void NotifyEPMRequestStatus(RequestStatus status,
                            void* profile_id,
                            int process_id,
                            int render_frame_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  extensions::ProcessManager* process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  // This may be NULL in unit tests.
  if (!process_manager)
    return;

  // Will be NULL if the request was not issued on behalf of a renderer (e.g. a
  // system-level request).
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(process_id, render_frame_id);
  if (render_frame_host) {
    if (status == REQUEST_STARTED) {
      process_manager->OnNetworkRequestStarted(render_frame_host);
    } else if (status == REQUEST_DONE) {
      process_manager->OnNetworkRequestDone(render_frame_host);
    } else {
      NOTREACHED();
    }
  }
}

void ForwardRequestStatus(
    RequestStatus status, net::URLRequest* request, void* profile_id) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return;

  int process_id, render_frame_id;
  if (info->GetAssociatedRenderFrame(&process_id, &render_frame_id)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyEPMRequestStatus,
                   status, profile_id, process_id, render_frame_id));
  }
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
#if defined(OS_ANDROID)
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
#endif  // defined(OS_ANDROID)
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
    : event_router_(event_router),
      profile_(NULL),
      enable_referrers_(enable_referrers),
      enable_do_not_track_(NULL),
      force_google_safe_search_(NULL),
      url_blacklist_manager_(NULL),
      domain_reliability_monitor_(NULL),
      received_content_length_(0),
      original_content_length_(0),
      first_request_(true) {
  DCHECK(event_router);
  DCHECK(enable_referrers);
}

ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

void ChromeNetworkDelegate::set_extension_info_map(
    extensions::InfoMap* extension_info_map) {
  extension_info_map_ = extension_info_map;
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
void ChromeNetworkDelegate::NeverThrottleRequests() {
  g_never_throttle_requests_ = true;
}

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
#if defined(DATA_REDUCTION_PROXY_PROBE_URL)
    record_timing = (request->url() != GURL(DATA_REDUCTION_PROXY_PROBE_URL));
#endif
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

  ForwardRequestStatus(REQUEST_STARTED, request, profile_);

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

  int rv = ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      profile_, extension_info_map_.get(), request, wrapped_callback,
      new_url);

  if (force_safe_search && rv == net::OK && new_url->is_empty())
    ForceGoogleSafeSearch(request, new_url);

  if (connect_interceptor_)
    connect_interceptor_->WitnessURLRequest(request);

  return rv;
}

int ChromeNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  TRACE_EVENT_ASYNC_STEP_PAST0("net", "URLRequest", request, "SendRequest");
  return ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
      profile_, extension_info_map_.get(), request, callback, headers);
}

void ChromeNetworkDelegate::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      profile_, extension_info_map_.get(), request, headers);
}

int ChromeNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      profile_,
      extension_info_map_.get(),
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
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      profile_, extension_info_map_.get(), request, new_location);
}


void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  TRACE_EVENT_ASYNC_STEP_PAST0("net", "URLRequest", request, "ResponseStarted");
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      profile_, extension_info_map_.get(), request);
  ForwardProxyErrors(request, event_router_.get(), profile_);
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

    bool is_redirect = request->response_headers() &&
        net::HttpResponseHeaders::IsRedirectResponseCode(
            request->response_headers()->response_code());
    if (!is_redirect) {
      ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
          profile_, extension_info_map_.get(), request);
    }
  } else if (request->status().status() == net::URLRequestStatus::FAILED ||
             request->status().status() == net::URLRequestStatus::CANCELED) {
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
            profile_, extension_info_map_.get(), request, started);
  } else {
    NOTREACHED();
  }
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnCompleted(request, started);
  ForwardProxyErrors(request, event_router_.get(), profile_);

  ForwardRequestStatus(REQUEST_DONE, request, profile_);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnURLRequestDestroyed(
      profile_, request);
}

void ChromeNetworkDelegate::OnPACScriptError(int line_number,
                                             const base::string16& error) {
  extensions::ProxyEventRouter::GetInstance()->OnPACScriptError(
      event_router_.get(), profile_, line_number, error);
}

net::NetworkDelegate::AuthRequiredResponse
ChromeNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      profile_, extension_info_map_.get(), request, auth_info,
      callback, credentials);
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
      "/media",
      "/opt/oem",
      "/usr/share/chromeos-assets",
      "/tmp",
      "/var/log",
  };

  // The actual location of "/home/chronos/user/Downloads" is the Downloads
  // directory under the profile path ("/home/chronos/user' is a hard link to
  // current primary logged in profile.) For the support of multi-profile
  // sessions, we are switching to use explicit "$PROFILE_PATH/Downloads" path
  // and here whitelist such access.
  if (!profile_path_.empty()) {
    const base::FilePath downloads = profile_path_.AppendASCII("Downloads");
    if (downloads == path.StripTrailingSeparators() || downloads.IsParent(path))
      return true;
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
  if (g_never_throttle_requests_) {
    return false;
  }

  return request.first_party_for_cookies().scheme() ==
      extensions::kExtensionScheme;
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
