// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include <stddef.h>
#include <stdlib.h>

#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/net/chrome_extensions_network_delegate.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/request_source_bandwidth_histograms.h"
#include "chrome/browser/net/safe_search_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/domain_reliability/monitor.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log.h"
#include "net/url_request/url_request.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/io_thread.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "components/precache/content/precache_manager.h"
#endif

#if defined(OS_CHROMEOS)
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

#if BUILDFLAG(ANDROID_JAVA_UI)
void RecordPrecacheStatsOnUIThread(const GURL& url,
                                   const GURL& referrer,
                                   base::TimeDelta latency,
                                   const base::Time& fetch_time,
                                   int64_t size,
                                   bool was_cached,
                                   void* profile_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_browser_process->profile_manager()->IsValidProfile(profile_id))
    return;
  Profile* profile = reinterpret_cast<Profile*>(profile_id);

  precache::PrecacheManager* precache_manager =
      precache::PrecacheManagerFactory::GetForBrowserContext(profile);
  // |precache_manager| could be NULL if the profile is off the record.
  if (!precache_manager || !precache_manager->IsPrecachingAllowed())
    return;

  precache_manager->RecordStatsForFetch(url, referrer, latency, fetch_time,
                                        size, was_cached);
}
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

void ReportInvalidReferrerSendOnUI() {
  base::RecordAction(
      base::UserMetricsAction("Net.URLRequest_StartJob_InvalidReferrer"));
}

void ReportInvalidReferrerSend(const GURL& target_url,
                               const GURL& referrer_url) {
  LOG(ERROR) << "Cancelling request to " << target_url
             << " with invalid referrer " << referrer_url;
  // Record information to help debug http://crbug.com/422871
  if (!target_url.SchemeIsHTTPOrHTTPS())
    return;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&ReportInvalidReferrerSendOnUI));
  base::debug::DumpWithoutCrashing();
  NOTREACHED();
}

// Record network errors that HTTP requests complete with, including OK and
// ABORTED.
void RecordNetworkErrorHistograms(const net::URLRequest* request) {
  if (request->url().SchemeIs("http")) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.HttpRequestCompletionErrorCodes",
                                std::abs(request->status().error()));

    if (request->load_flags() & net::LOAD_MAIN_FRAME) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Net.HttpRequestCompletionErrorCodes.MainFrame",
          std::abs(request->status().error()));
    }
  }
}

// Returns whether |request| is likely to be eligible for delta-encoding.
// This is only a rough approximation right now, based on MIME type.
bool CanRequestBeDeltaEncoded(const net::URLRequest* request) {
  struct {
    const char *prefix;
    const char *suffix;
  } kEligibleMasks[] = {
    // All text/ types are eligible, even if not displayable.
    { "text/", NULL },
    // JSON (application/json and application/*+json) is eligible.
    { "application/", "json" },
    // Javascript is eligible.
    { "application/", "javascript" },
    // XML (application/xml and application/*+xml) is eligible.
    { "application/", "xml" },
  };

  std::string mime_type;
  request->GetMimeType(&mime_type);

  for (size_t i = 0; i < arraysize(kEligibleMasks); i++) {
    const char *prefix = kEligibleMasks[i].prefix;
    const char *suffix = kEligibleMasks[i].suffix;
    if (prefix &&
        !base::StartsWith(mime_type, prefix, base::CompareCase::SENSITIVE))
      continue;
    if (suffix &&
        !base::EndsWith(mime_type, suffix, base::CompareCase::SENSITIVE))
      continue;
    return true;
  }
  return false;
}

// Returns whether |request| was issued by a renderer process, as opposed to
// the browser process or a plugin process.
bool IsRendererInitiatedRequest(const net::URLRequest* request) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  return info && info->GetProcessType() == content::PROCESS_TYPE_RENDERER;
}

// Uploads UMA histograms for delta encoding eligibility. This method can only
// be safely called after the network stack has called both OnStarted and
// OnCompleted, since it needs the received response content length and the
// response headers.
void RecordCacheStateStats(const net::URLRequest* request) {
  net::HttpRequestHeaders request_headers;
  if (!request->GetFullRequestHeaders(&request_headers)) {
    // GetFullRequestHeaders is guaranteed to succeed if OnResponseStarted() has
    // been called on |request|, so if GetFullRequestHeaders() fails,
    // RecordCacheStateStats must have been called before
    // OnResponseStarted().
    return;
  }

  if (!IsRendererInitiatedRequest(request)) {
    // Ignore browser-initiated requests. These are internal requests like safe
    // browsing and sync, and so on. Some of these could be eligible for
    // delta-encoding, but to be conservative this function ignores all of them.
    return;
  }

  const int kCacheAffectingFlags = net::LOAD_BYPASS_CACHE |
                                   net::LOAD_DISABLE_CACHE |
                                   net::LOAD_PREFERRING_CACHE;

  if (request->load_flags() & kCacheAffectingFlags) {
    // Ignore requests with cache-affecting flags, which would otherwise mess up
    // these stats.
    return;
  }

  enum {
    CACHE_STATE_FROM_CACHE,
    CACHE_STATE_STILL_VALID,
    CACHE_STATE_NO_LONGER_VALID,
    CACHE_STATE_NO_ENTRY,
    CACHE_STATE_MAX,
  } state = CACHE_STATE_NO_ENTRY;
  bool had_cache_headers =
      request_headers.HasHeader(net::HttpRequestHeaders::kIfModifiedSince) ||
      request_headers.HasHeader(net::HttpRequestHeaders::kIfNoneMatch) ||
      request_headers.HasHeader(net::HttpRequestHeaders::kIfRange);
  if (request->was_cached() && !had_cache_headers) {
    // Entry was served directly from cache.
    state = CACHE_STATE_FROM_CACHE;
  } else if (request->was_cached() && had_cache_headers) {
    // Expired entry was present in cache, and server responded with NOT
    // MODIFIED, indicating the expired entry is still valid.
    state = CACHE_STATE_STILL_VALID;
  } else if (!request->was_cached() && had_cache_headers) {
    // Expired entry was present in cache, and server responded with something
    // other than NOT MODIFIED, indicating the entry is no longer valid.
    state = CACHE_STATE_NO_LONGER_VALID;
  } else if (!request->was_cached() && !had_cache_headers) {
    // Neither |was_cached| nor |had_cache_headers|, so there's no local cache
    // entry for this content at all.
    state = CACHE_STATE_NO_ENTRY;
  }

  UMA_HISTOGRAM_ENUMERATION("Net.CacheState.AllRequests", state,
                            CACHE_STATE_MAX);
  if (CanRequestBeDeltaEncoded(request)) {
    UMA_HISTOGRAM_ENUMERATION("Net.CacheState.EncodeableRequests", state,
                              CACHE_STATE_MAX);
  }

  int64_t size = request->received_response_content_length();
  if (size >= 0 && state == CACHE_STATE_NO_LONGER_VALID) {
    UMA_HISTOGRAM_COUNTS("Net.CacheState.AllBytes", size);
    if (CanRequestBeDeltaEncoded(request)) {
      UMA_HISTOGRAM_COUNTS("Net.CacheState.EncodeableBytes", size);
    }
  }
}

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate(
    extensions::EventRouterForwarder* event_router,
    BooleanPrefMember* enable_referrers)
    : profile_(NULL),
      enable_referrers_(enable_referrers),
      enable_do_not_track_(NULL),
      force_google_safe_search_(NULL),
      force_youtube_safety_mode_(NULL),
#if defined(ENABLE_CONFIGURATION_POLICY)
      url_blacklist_manager_(NULL),
#endif
      domain_reliability_monitor_(NULL),
      experimental_web_platform_features_enabled_(
          base::CommandLine::ForCurrentProcess()
              ->HasSwitch(switches::kEnableExperimentalWebPlatformFeatures)),
      data_use_aggregator_(nullptr),
      is_data_usage_off_the_record_(true) {
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
    content_settings::CookieSettings* cookie_settings) {
  cookie_settings_ = cookie_settings;
}

void ChromeNetworkDelegate::set_predictor(
    chrome_browser_net::Predictor* predictor) {
  connect_interceptor_.reset(
      new chrome_browser_net::ConnectInterceptor(predictor));
}

void ChromeNetworkDelegate::set_data_use_aggregator(
    data_usage::DataUseAggregator* data_use_aggregator,
    bool is_data_usage_off_the_record) {
  data_use_aggregator_ = data_use_aggregator;
  is_data_usage_off_the_record_ = is_data_usage_off_the_record;
}

// static
void ChromeNetworkDelegate::InitializePrefsOnUIThread(
    BooleanPrefMember* enable_referrers,
    BooleanPrefMember* enable_do_not_track,
    BooleanPrefMember* force_google_safe_search,
    BooleanPrefMember* force_youtube_safety_mode,
    PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  enable_referrers->Init(prefs::kEnableReferrers, pref_service);
  enable_referrers->MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  if (enable_do_not_track) {
    enable_do_not_track->Init(prefs::kEnableDoNotTrack, pref_service);
    enable_do_not_track->MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }
  if (force_google_safe_search) {
    force_google_safe_search->Init(prefs::kForceGoogleSafeSearch, pref_service);
    force_google_safe_search->MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }
  if (force_youtube_safety_mode) {
    force_youtube_safety_mode->Init(prefs::kForceYouTubeSafetyMode,
                                    pref_service);
    force_youtube_safety_mode->MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }
}

// static
void ChromeNetworkDelegate::AllowAccessToAllFiles() {
  g_allow_file_access_ = true;
}

int ChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::ChromeNetworkDelegate::OnBeforeURLRequest"));

#if defined(ENABLE_CONFIGURATION_POLICY)
  // TODO(joaodasilva): This prevents extensions from seeing URLs that are
  // blocked. However, an extension might redirect the request to another URL,
  // which is not blocked.

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  int error = net::ERR_BLOCKED_BY_ADMINISTRATOR;
  if (info && content::IsResourceTypeFrame(info->GetResourceType()) &&
      url_blacklist_manager_ &&
      url_blacklist_manager_->ShouldBlockRequestForFrame(
          request->url(), &error)) {
    // URL access blocked by policy.
    request->net_log().AddEvent(
        net::NetLog::TYPE_CHROME_POLICY_ABORTED_REQUEST,
        net::NetLog::StringCallback("url",
                                    &request->url().possibly_invalid_spec()));
    return error;
  }
#endif

  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::ChromeNetworkDelegate::OnBeforeURLRequest 2"));

  extensions_delegate_->ForwardStartRequestStatus(request);

  if (!enable_referrers_->GetValue())
    request->SetReferrer(std::string());
  if (enable_do_not_track_ && enable_do_not_track_->GetValue())
    request->SetExtraRequestHeaderByName(kDNTHeader, "1", true /* override */);

  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::ChromeNetworkDelegate::OnBeforeURLRequest 3"));

  bool force_safe_search =
      (force_google_safe_search_ && force_google_safe_search_->GetValue());

  net::CompletionCallback wrapped_callback = callback;
  if (force_safe_search) {
    wrapped_callback = base::Bind(&ForceGoogleSafeSearchCallbackWrapper,
                                  callback,
                                  base::Unretained(request),
                                  base::Unretained(new_url));
  }

  int rv = extensions_delegate_->OnBeforeURLRequest(
      request, wrapped_callback, new_url);

  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::ChromeNetworkDelegate::OnBeforeURLRequest 4"));

  if (force_safe_search && rv == net::OK && new_url->is_empty())
    safe_search_util::ForceGoogleSafeSearch(request, new_url);

  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::ChromeNetworkDelegate::OnBeforeURLRequest 5"));

  if (connect_interceptor_)
    connect_interceptor_->WitnessURLRequest(request);

  return rv;
}

int ChromeNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  if (force_youtube_safety_mode_ && force_youtube_safety_mode_->GetValue())
    safe_search_util::ForceYouTubeSafetyMode(request, headers);

  return extensions_delegate_->OnBeforeSendHeaders(request, callback, headers);
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
  return extensions_delegate_->OnHeadersReceived(
      request,
      callback,
      original_response_headers,
      override_response_headers,
      allowed_unsafe_redirect_url);
}

void ChromeNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                             const GURL& new_location) {
// Recording data use of request on redirects.
#if !defined(OS_IOS)
  data_use_measurement_.ReportDataUseUMA(request);
#endif
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnBeforeRedirect(request);
  extensions_delegate_->OnBeforeRedirect(request, new_location);
}


void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  extensions_delegate_->OnResponseStarted(request);
}

void ChromeNetworkDelegate::OnNetworkBytesReceived(net::URLRequest* request,
                                                   int64_t bytes_received) {
#if defined(ENABLE_TASK_MANAGER)
  // Note: Currently, OnNetworkBytesReceived is only implemented for HTTP jobs,
  // not FTP or other types, so those kinds of bytes will not be reported here.
  task_management::TaskManagerInterface::OnRawBytesRead(*request,
                                                        bytes_received);
#endif  // defined(ENABLE_TASK_MANAGER)

  ReportDataUsageStats(request, 0 /* tx_bytes */, bytes_received);
}

void ChromeNetworkDelegate::OnNetworkBytesSent(net::URLRequest* request,
                                               int64_t bytes_sent) {
  ReportDataUsageStats(request, bytes_sent, 0 /* rx_bytes */);
}

void ChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                        bool started) {
#if !defined(OS_IOS)
  // TODO(amohammadkhan): Verify that there is no double recording in data use
  // of redirected requests.
  data_use_measurement_.ReportDataUseUMA(request);
#endif
  RecordNetworkErrorHistograms(request);
  if (started) {
    // Only call in for requests that were started, to obey the precondition
    // that RecordCacheStateStats can only be called on requests for which
    // OnResponseStarted was called.
    RecordCacheStateStats(request);
  }

  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
#if BUILDFLAG(ANDROID_JAVA_UI)
    // For better accuracy, we use the actual bytes read instead of the length
    // specified with the Content-Length header, which may be inaccurate,
    // or missing, as is the case with chunked encoding.
    int64_t received_content_length =
        request->received_response_content_length();
    base::TimeDelta latency = base::TimeTicks::Now() - request->creation_time();

    // Record precache metrics when a fetch is completed successfully, if
    // precaching is allowed.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RecordPrecacheStatsOnUIThread, request->url(),
                   GURL(request->referrer()), latency, base::Time::Now(),
                   received_content_length, request->was_cached(), profile_));
#endif  // BUILDFLAG(ANDROID_JAVA_UI)
    extensions_delegate_->OnCompleted(request, started);
  } else if (request->status().status() == net::URLRequestStatus::FAILED ||
             request->status().status() == net::URLRequestStatus::CANCELED) {
    extensions_delegate_->OnCompleted(request, started);
  } else {
    NOTREACHED();
  }
  if (domain_reliability_monitor_)
    domain_reliability_monitor_->OnCompleted(request, started);
  RecordRequestSourceBandwidth(request, started);
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
  if (content::ResourceRequestInfo::GetRenderFrameForRequest(
          &request, &render_process_id, &render_frame_id)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::CookiesRead,
                   render_process_id, render_frame_id,
                   request.url(), request.first_party_for_cookies(),
                   cookie_list, !allow));
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
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
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

  // Allow to load offline pages, which are stored in the $PROFILE_PATH/Offline
  // Pages/archives.
  if (!profile_path_.empty()) {
    const base::FilePath offline_page_archives =
        profile_path_.Append(chrome::kOfflinePageArchviesDirname);
    if (offline_page_archives.IsParent(path))
      return true;
  }

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

bool ChromeNetworkDelegate::OnAreExperimentalCookieFeaturesEnabled() const {
  return experimental_web_platform_features_enabled_;
}

bool ChromeNetworkDelegate::OnAreStrictSecureCookiesEnabled() const {
  const std::string enforce_strict_secure_group =
      base::FieldTrialList::FindFullName("StrictSecureCookies");
  return experimental_web_platform_features_enabled_ ||
         base::StartsWith(enforce_strict_secure_group, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool ChromeNetworkDelegate::OnCancelURLRequestWithPolicyViolatingReferrerHeader(
    const net::URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  ReportInvalidReferrerSend(target_url, referrer_url);
  return true;
}

void ChromeNetworkDelegate::ReportDataUsageStats(net::URLRequest* request,
                                                 int64_t tx_bytes,
                                                 int64_t rx_bytes) {
  if (!data_use_aggregator_)
    return;

  if (is_data_usage_off_the_record_) {
    data_use_aggregator_->ReportOffTheRecordDataUse(tx_bytes, rx_bytes);
    return;
  }

  data_use_aggregator_->ReportDataUse(request, tx_bytes, rx_bytes);
}
