// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include <stdlib.h>

#include <vector>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/load_time_stats.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
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
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/url_blacklist_manager.h"
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
  if (!google_util::IsGoogleSearchUrl(request->url().spec()) &&
      !google_util::IsGoogleHomePageUrl(request->url().spec()))
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

// Notifies the ExtensionProcessManager that a request has started or stopped
// for a particular RenderView.
void NotifyEPMRequestStatus(RequestStatus status,
                            void* profile_id,
                            int process_id,
                            int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  ExtensionProcessManager* extension_process_manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  // This may be NULL in unit tests.
  if (!extension_process_manager)
    return;

  // Will be NULL if the request was not issued on behalf of a renderer (e.g. a
  // system-level request).
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(process_id, render_view_id);
  if (render_view_host) {
    if (status == REQUEST_STARTED) {
      extension_process_manager->OnNetworkRequestStarted(render_view_host);
    } else if (status == REQUEST_DONE) {
      extension_process_manager->OnNetworkRequestDone(render_view_host);
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

  int process_id, render_view_id;
  if (info->GetAssociatedRenderView(&process_id, &render_view_id)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyEPMRequestStatus,
                   status, profile_id, process_id, render_view_id));
  }
}

#if defined(OS_ANDROID)
// Increments an int64, stored as a string, in a ListPref at the specified
// index.  The value must already exist and be a string representation of a
// number.
void AddInt64ToListPref(size_t index, int64 length,
                        ListPrefUpdate& list_update) {
  int64 value = 0;
  std::string old_string_value;
  bool rv = list_update->GetString(index, &old_string_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(old_string_value, &value);
    DCHECK(rv);
  }
  value += length;
  list_update->Set(index, Value::CreateStringValue(base::Int64ToString(value)));
}
#endif  // defined(OS_ANDROID)

void UpdateContentLengthPrefs(int received_content_length,
                              int original_content_length) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(received_content_length, 0);
  DCHECK_GE(original_content_length, 0);

  // Can be NULL in a unit test.
  if (!g_browser_process)
    return;

  PrefService* prefs = g_browser_process->local_state();
  if (!prefs)
    return;

  int64 total_received = prefs->GetInt64(prefs::kHttpReceivedContentLength);
  int64 total_original = prefs->GetInt64(prefs::kHttpOriginalContentLength);
  total_received += received_content_length;
  total_original += original_content_length;
  prefs->SetInt64(prefs::kHttpReceivedContentLength, total_received);
  prefs->SetInt64(prefs::kHttpOriginalContentLength, total_original);

#if defined(OS_ANDROID)
  base::Time now = base::Time::Now().LocalMidnight();
  const size_t kNumDaysInHistory = 60;

  // Each day, we calculate the total number of bytes received and the total
  // size of all corresponding resources before any data-reducing recompression
  // is applied. These values are used to compute the data savings realized
  // by applying our compression techniques. Totals for the last
  // |kNumDaysInHistory| days are maintained.
  ListPrefUpdate original_update(prefs, prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(prefs, prefs::kDailyHttpReceivedContentLength);

  // Determine how many days it has been since the last update.
  int64 then_internal = prefs->GetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time then = base::Time::FromInternalValue(then_internal);
  int days_since_last_update = (now - then).InDays();

  if (days_since_last_update) {
    // Record the last update time.
    prefs->SetInt64(prefs::kDailyHttpContentLengthLastUpdateDate,
                    now.ToInternalValue());

    if (days_since_last_update == -1) {
      // The system may go backwards in time by up to a day for legitimate
      // reasons, such as with changes to the time zone. In such cases the
      // history is likely still valid. Shift backwards one day and retain all
      // values.
      original_update->Remove(kNumDaysInHistory - 1, NULL);
      received_update->Remove(kNumDaysInHistory - 1, NULL);
      original_update->Insert(0, new StringValue(base::Int64ToString(0)));
      received_update->Insert(0, new StringValue(base::Int64ToString(0)));
      days_since_last_update = 0;

    } else if (days_since_last_update < -1) {
      // Erase all entries if the system went backwards in time by more than
      // a day.
      original_update->Clear();
      days_since_last_update = kNumDaysInHistory;
    }

    // Add entries for days since last update.
    for (int i = 0;
         i < days_since_last_update && i < static_cast<int>(kNumDaysInHistory);
         ++i) {
      original_update->AppendString(base::Int64ToString(0));
      received_update->AppendString(base::Int64ToString(0));
    }

    // Maintain the invariant that there should never be more than
    // |kNumDaysInHistory| days in the histories.
    while (original_update->GetSize() > kNumDaysInHistory)
      original_update->Remove(0, NULL);
    while (received_update->GetSize() > kNumDaysInHistory)
      received_update->Remove(0, NULL);
  }
  DCHECK_EQ(kNumDaysInHistory, original_update->GetSize());
  DCHECK_EQ(kNumDaysInHistory, received_update->GetSize());

  // Update the counts for the current day.
  AddInt64ToListPref(kNumDaysInHistory - 1,
                     original_content_length, original_update);
  AddInt64ToListPref(kNumDaysInHistory - 1,
                     received_content_length, received_update);
#endif
}

void StoreAccumulatedContentLength(int received_content_length,
                                   int original_content_length) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateContentLengthPrefs,
                 received_content_length, original_content_length));
}

void RecordContentLengthHistograms(
    int64 received_content_length, int64 original_content_length) {
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
#endif
}

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
      load_time_stats_(NULL),
      received_content_length_(0),
      original_content_length_(0) {
  DCHECK(event_router);
  DCHECK(enable_referrers);
}

ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

void ChromeNetworkDelegate::set_extension_info_map(
    ExtensionInfoMap* extension_info_map) {
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
Value* ChromeNetworkDelegate::HistoricNetworkStatsInfoToValue() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* prefs = g_browser_process->local_state();
  int64 total_received = prefs->GetInt64(prefs::kHttpReceivedContentLength);
  int64 total_original = prefs->GetInt64(prefs::kHttpOriginalContentLength);

  DictionaryValue* dict = new DictionaryValue();
  // Use strings to avoid overflow.  base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return dict;
}

Value* ChromeNetworkDelegate::SessionNetworkStatsInfoToValue() const {
  DictionaryValue* dict = new DictionaryValue();
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
#if defined(ENABLE_CONFIGURATION_POLICY)
  // TODO(joaodasilva): This prevents extensions from seeing URLs that are
  // blocked. However, an extension might redirect the request to another URL,
  // which is not blocked.
  if (url_blacklist_manager_ &&
      url_blacklist_manager_->IsRequestBlocked(*request)) {
    // URL access blocked by policy.
    request->net_log().AddEvent(
        net::NetLog::TYPE_CHROME_POLICY_ABORTED_REQUEST,
        net::NetLog::StringCallback("url",
                                    &request->url().possibly_invalid_spec()));
    return net::ERR_BLOCKED_BY_ADMINISTRATOR;
  }
#endif

  ForwardRequestStatus(REQUEST_STARTED, request, profile_);

  if (!enable_referrers_->GetValue())
    request->set_referrer(std::string());
  if (enable_do_not_track_ && enable_do_not_track_->GetValue())
    request->SetExtraRequestHeaderByName(kDNTHeader, "1", true /* override */);

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
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
  return ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      profile_, extension_info_map_.get(), request, callback,
      original_response_headers, override_response_headers);
}

void ChromeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                             const GURL& new_location) {
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      profile_, extension_info_map_.get(), request, new_location);
}


void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      profile_, extension_info_map_.get(), request);
  ForwardProxyErrors(request, event_router_.get(), profile_);
}

void ChromeNetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                           int bytes_read) {
  performance_monitor::PerformanceMonitor::GetInstance()->BytesReadOnIOThread(
      request, bytes_read);

#if defined(ENABLE_TASK_MANAGER)
  TaskManager::GetInstance()->model()->NotifyBytesRead(request, bytes_read);
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                        bool started) {
  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    // For better accuracy, we use the actual bytes read instead of the length
    // specified with the Content-Length header, which may be inaccurate,
    // or missing, as is the case with chunked encoding.
    int64 received_content_length = request->received_response_content_length();

    // Only record for http or https urls.
    bool is_http = request->url().SchemeIs("http");
    bool is_https = request->url().SchemeIs("https");

    if (!request->was_cached() &&         // Don't record cached content
        received_content_length &&        // Zero-byte responses aren't useful.
        (is_http || is_https)) {          // Only record for HTTP or HTTPS urls.
      int64 original_content_length =
          request->response_info().headers->GetInt64HeaderValue(
              "x-original-content-length");
      // Since there was no indication of the original content length, presume
      // it is no different from the number of bytes read.
      int64 adjusted_original_content_length = original_content_length;
      if (adjusted_original_content_length == -1)
        adjusted_original_content_length = received_content_length;
      AccumulateContentLength(received_content_length,
                              adjusted_original_content_length);
      RecordContentLengthHistograms(received_content_length,
                                    original_content_length);
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
  ForwardProxyErrors(request, event_router_.get(), profile_);

  ForwardRequestStatus(REQUEST_DONE, request, profile_);
}

void ChromeNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnURLRequestDestroyed(
      profile_, request);
  if (load_time_stats_)
    load_time_stats_->OnURLRequestDestroyed(*request);
}

void ChromeNetworkDelegate::OnPACScriptError(int line_number,
                                             const string16& error) {
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
  if (!cookie_settings_)
    return true;

  bool allow = cookie_settings_->IsReadingCookieAllowed(
      request.url(), request.first_party_for_cookies());

  int render_process_id = -1;
  int render_view_id = -1;
  if (content::ResourceRequestInfo::GetRenderViewForRequest(
          &request, &render_process_id, &render_view_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookiesRead,
                   render_process_id, render_view_id,
                   request.url(), request.first_party_for_cookies(),
                   cookie_list, !allow));
  }

  return allow;
}

bool ChromeNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                           const std::string& cookie_line,
                                           net::CookieOptions* options) {
  // NULL during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return true;

  bool allow = cookie_settings_->IsSettingCookieAllowed(
      request.url(), request.first_party_for_cookies());

  int render_process_id = -1;
  int render_view_id = -1;
  if (content::ResourceRequestInfo::GetRenderViewForRequest(
          &request, &render_process_id, &render_view_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookieChanged,
                   render_process_id, render_view_id,
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
  if (!base::chromeos::IsRunningOnChromeOS() ||
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

void ChromeNetworkDelegate::OnRequestWaitStateChange(
    const net::URLRequest& request,
    RequestWaitState state) {
  if (load_time_stats_)
    load_time_stats_->OnRequestWaitStateChange(request, state);
}

void ChromeNetworkDelegate::AccumulateContentLength(
    int64 received_content_length, int64 original_content_length) {
  DCHECK_GE(received_content_length, 0);
  DCHECK_GE(original_content_length, 0);
  StoreAccumulatedContentLength(received_content_length,
                                original_content_length);
  received_content_length_ += received_content_length;
  original_content_length_ += original_content_length;
}
